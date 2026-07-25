#include "svc_ble_sync.h"
#include "app_events.h"
#include "cJSON.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "esp_random.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "ui.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"
#include "svc_screen_sleep.h"
#include "svc_vibro.h"
#include "sys/time.h"
#include <freertos/queue.h>

void ble_store_config_init(void);

static const char *TAG = "SVC_BLE";

#define BLE_SVC_UUID_TIME_SYNC 0x1234
#define BLE_CHR_UUID_TIME_SYNC 0x5678
#define BLE_CHR_UUID_NOTIFY 0x5679 // Телефон пишет текст сюда
#define BLE_CHR_UUID_ACTION 0x5680 // Часы шлют кнопки отсюда
#define BLE_CHR_UUID_LOG_STREAM 0x5681



static uint16_t action_chr_val_handle = 0; // Хэндл характеристики
static uint16_t active_conn_handle = 0;    // Хэндл подключения
static uint16_t time_sync_conn_handle;
static uint8_t own_addr_type;

static uint16_t log_chr_val_handle = 0;
static QueueHandle_t log_queue = NULL;
static bool log_subscribed = false;  // Телефон подписался на уведомления?

static int (*original_vprintf)(const char *fmt, va_list args) = NULL;

// Структура пакета лога
typedef struct {
    uint32_t timestamp_ms;
    uint8_t level;
    char tag[12];    // Короткий тег (большинство тегов в проекте ~8-10 символов)
    char msg[180];   // Сообщение с запасом
} ble_log_item_t;

// Состояние
static bool ble_running = false;
static bool ble_advertising = false;
static bool ble_initialized = false;

static void ble_advertise(void);

static volatile bool ble_bonds_loaded = false; // Флаг готовности

// Кастомный коллбэк для отслеживания загрузки хранилища
static int ble_store_load_cb(int obj_type, union ble_store_value *val, void *arg) {
    // Просто считаем количество загруженных ключей для отладки
    static int loaded_count = 0;
    if (obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
        loaded_count++;
        ESP_LOGD(TAG, "Загружен ключ #%d для %02X:%02X:...", loaded_count, val->sec.peer_addr.val[5],
                 val->sec.peer_addr.val[4]);
    }
    return 0;
}

/**
 * @brief Парсит строку лога в формате ESP-IDF: "I (1234) TAG: message"
 * @return true если успешно распаршено
 */
// Возвращает: 1 = распарсено успешно, 0 = не распарсилось, -1 = отфильтровано (пропустить)
static int parse_esp_log_line(const char *full_msg, ble_log_item_t *item) {
    if (strlen(full_msg) < 10) return 0;
    
    char level_char = full_msg[0];
    item->level = ESP_LOG_INFO;
    if (level_char == 'E') item->level = ESP_LOG_ERROR;
    else if (level_char == 'W') item->level = ESP_LOG_WARN;
    else if (level_char == 'D') item->level = ESP_LOG_DEBUG;

    const char *paren_close = strchr(full_msg, ')');
    if (!paren_close) return 0;
    
    const char *after_paren = paren_close + 1;
    while (*after_paren == ' ') after_paren++;
    
    const char *colon = strchr(after_paren, ':');
    if (!colon) return 0;
    
    size_t tag_len = colon - after_paren;
    if (tag_len == 0 || tag_len >= sizeof(item->tag)) return 0;
    
    strncpy(item->tag, after_paren, tag_len);
    item->tag[tag_len] = '\0';

    // ★ ФИЛЬТР: возвращаем -1 (пропустить), а не 0!
    if (strcmp(item->tag, "NimBLE") == 0) return -1;
    if (strcmp(item->tag, "esp_image") == 0) return -1;
    if (strcmp(item->tag, "boot") == 0) return -1;
    if (strcmp(item->tag, "phy_init") == 0) return -1;
    if (strcmp(item->tag, "heap_init") == 0) return -1;
    if (strcmp(item->tag, "wifi") == 0) return -1;
    if (strcmp(item->tag, "BT_PHY") == 0) return -1;
    if (strcmp(item->tag, "BT_BTM") == 0) return -1;
    if (strcmp(item->tag, "BT_L2CAP") == 0) return -1;
    if (strcmp(item->tag, "BT_SMP") == 0) return -1;
    if (strcmp(item->tag, "BT_GATT") == 0) return -1;
    
    const char *msg_start = colon + 1;
    while (*msg_start == ' ') msg_start++;
    
    // ★ Фильтр по содержимому (тоже -1)
    if (strstr(msg_start, "GATT procedure initiated") != NULL) return -1;
    if (strstr(msg_start, "att_handle=") != NULL) return -1;
    
    strncpy(item->msg, msg_start, sizeof(item->msg) - 1);
    item->msg[sizeof(item->msg) - 1] = '\0';
    
    return 1; // успешно распарсено
}

/**
 * @brief Исправленный перехватчик (буферизация ДО использования args)
 */
static int ble_log_hook_vprintf(const char *fmt, va_list args) {
    // 1. Буферизируем сообщение
    char full_msg[256];
    va_list args_copy;
    va_copy(args_copy, args);
    vsnprintf(full_msg, sizeof(full_msg), fmt, args_copy);
    va_end(args_copy);

    // 2. Вывод в UART
    int ret = 0;
    if (original_vprintf) {
        ret = original_vprintf(fmt, args);
    }

    // ★ ОТЛАДКА: покажем, что хук вызван
    static int hook_call_count = 0;
    hook_call_count++;
    if (hook_call_count <= 10) {  // Первые 10 вызовов
        printf("[HOOK #%d] ble_running=%d, log_subscribed=%d, queue=%p\n", 
               hook_call_count, ble_running, log_subscribed, log_queue);
    }

    // 3. Проверки
    if (!ble_running) {
        if (hook_call_count <= 5) {
            printf("[HOOK] SKIP: ble_running=false\n");
        }
        return ret;
    }
    if (!log_subscribed) {
        if (hook_call_count <= 5) {
            printf("[HOOK] SKIP: log_subscribed=false\n");
        }
        return ret;
    }
    if (log_queue == NULL) {
        if (hook_call_count <= 5) {
            printf("[HOOK] SKIP: log_queue=NULL\n");
        }
        return ret;
    }

    // 4. Парсим строку
    ble_log_item_t *item = malloc(sizeof(ble_log_item_t));
    if (!item) return ret;

    item->timestamp_ms = esp_timer_get_time() / 1000;

    int parse_result = parse_esp_log_line(full_msg, item);
    if (parse_result == -1) {
        // ★ ОТФИЛЬТРОВАНО — просто пропускаем, НЕ отправляем!
        free(item);
        return ret;
    }
    if (parse_result == 0) {
        // Не удалось распарсить — отправляем как есть
        item->level = ESP_LOG_INFO;
        strncpy(item->tag, "APP", sizeof(item->tag) - 1);
        strncpy(item->msg, full_msg, sizeof(item->msg) - 1);
        item->msg[sizeof(item->msg) - 1] = '\0';
    }
    // parse_result == 1 → уже заполнено, отправляем

    // 6. Отправка в очередь
    if (xQueueIsQueueFullFromISR(log_queue)) {
        ble_log_item_t *dummy;
        xQueueReceive(log_queue, &dummy, 0);
        if (dummy) free(dummy);
    }

    if (xQueueSend(log_queue, &item, 0) != pdTRUE) {
        free(item);
    }

    return ret;
}

/**
 * @brief Задача отправки накопленных логов в BLE
 */
static void ble_log_sender_task(void *param) {
    ble_log_item_t *item;
    uint8_t tx_buffer[240];  // С запасом под MTU=247

    while (1) {
        // Ждём элемент из очереди (с таймаутом для периодических проверок)
        if (xQueueReceive(log_queue, &item, pdMS_TO_TICKS(50)) == pdTRUE && item) {
            
            // Проверяем актуальность подключения
            if (!log_subscribed || active_conn_handle == 0) {
                free(item);
                continue;
            }

            // Формируем пакет: [1B level][4B timestamp][N bytes tag:msg]
            uint16_t pos = 0;
            tx_buffer[pos++] = item->level;
            memcpy(tx_buffer + pos, &item->timestamp_ms, 4);
            pos += 4;

            // Кодируем "TAG:msg" с нуль-терминатором для простоты парсинга на телефоне
            int written = snprintf((char*)tx_buffer + pos, 
                                  sizeof(tx_buffer) - pos, 
                                  "%s:%s", item->tag, item->msg);
            pos += written;
            if (pos >= sizeof(tx_buffer) - 1) pos = sizeof(tx_buffer) - 1;
            tx_buffer[pos++] = '\0';  // Терминатор для удобства на телефоне

            // Отправляем NOTIFY
            struct os_mbuf *om = ble_hs_mbuf_from_flat(tx_buffer, pos);
            if (om) {
                int rc = ble_gatts_notify_custom(active_conn_handle, 
                                                 log_chr_val_handle, om);
                if (rc != 0) {
                    ESP_LOGD(TAG, "Notify failed: %d", rc);
                }
            }

            free(item);  // Освобождаем память
        }
        
        // Опционально: можно добавить "пульс" раз в секунду, если нет логов
        // но подписка активна — для keep-alive
    }
}

/**
 * @brief Инициализация BLE-логгера (вызвать в ble_do_init)
 */
void ble_log_stream_init(void) {
    log_queue = xQueueCreate(20, sizeof(ble_log_item_t*));
    if (!log_queue) {
        ESP_LOGE(TAG, "❌ Failed to create log queue!");
        return;
    }
    ESP_LOGI(TAG, "✅ Log queue created");

    original_vprintf = esp_log_set_vprintf(ble_log_hook_vprintf);
    if (original_vprintf == NULL) {
        ESP_LOGW(TAG, "⚠ Original vprintf was NULL (first hook?)");
    } else {
        ESP_LOGI(TAG, "✅ Log hook installed");
    }
}

/**
 * @brief Запуск задачи-отправителя (вызвать после nimble_port_freertos_init)
 */
void ble_log_sender_start(void) {
    xTaskCreate(ble_log_sender_task, "ble_log_snd", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "BLE log sender task started");
}

// Функция ожидания полной загрузки ключей
static void wait_for_bonds_loaded(void) {
    // Даём до 500мс на загрузку ключей из NVS
    for (int i = 0; i < 50 && !ble_bonds_loaded; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (ble_bonds_loaded) {
        ESP_LOGI(TAG, "✓ Ключи сопряжения загружены из NVS");
    } else {
        ESP_LOGW(TAG, "⚠ Таймаут загрузки ключей (возможно, нет сохранённых устройств)");
    }
}

// ==========================================
// GATT CALLBACK
// ==========================================

static int time_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len == 4) {
            uint32_t epoch_time;
            memcpy(&epoch_time, ctxt->om->om_data, 4);
            ESP_LOGI(TAG, "Получено время от телефона: %lu", epoch_time);

            struct timeval tv = {.tv_sec = epoch_time, .tv_usec = 0};
            settimeofday(&tv, NULL);
            esp_event_post(APP_EVENTS, EVT_BLE_TIME_SYNCED, NULL, 0, pdMS_TO_TICKS(100));
            svc_vibro_send_event(VIBRATION_EVENT_SINGLE_SHORT, LEVEL_MEDIUM, DURATION_DEFAULT);
            return 0;
        } else {
            ESP_LOGE(TAG, "Неверный размер пакета!");
            return BLE_ATT_ERR_UNLIKELY;
        }
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int notify_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        

        // 1. Безопасно копируем данные из mbuf (учитывая фрагментацию NimBLE)
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len > 512)
            len = 512; // Защита от переполнения

        char buf[513];
        int rc = os_mbuf_copydata(ctxt->om, 0, len, buf);
        if (rc != 0) {
            ESP_LOGE(TAG, "Ошибка копирования mbuf");
            return BLE_ATT_ERR_UNLIKELY;
        }
        buf[len] = '\0';

        ESP_LOGI(TAG, "Получено JSON (%d байт): %s", len, buf);

        // 2. Парсим JSON
        cJSON *root = cJSON_Parse(buf);
        if (root == NULL) {
            ESP_LOGE(TAG, "Ошибка парсинга JSON");
            return 0; // Просто игнорируем кривой пакет
        }

        // 3. Получаем тип пакета "t"
        cJSON *type_item = cJSON_GetObjectItemCaseSensitive(root, "t");
        if (cJSON_IsString(type_item) && (type_item->valuestring != NULL)) {

            // ==========================================
            // ОБРАБОТКА НОВОГО УВЕДОМЛЕНИЯ (n_new)
            // ==========================================
            if (strcmp(type_item->valuestring, "n_new") == 0) {
                svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_LONG, LEVEL_HIGH, DURATION_LONG);
                cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "d");
                if (cJSON_IsObject(data)) {
                    notify_data n_data;
                    memset(&n_data, 0, sizeof(n_data));

                    // Безопасно извлекаем поля с проверкой на NULL
                    cJSON *id = cJSON_GetObjectItemCaseSensitive(data, "id");
                    cJSON *app = cJSON_GetObjectItemCaseSensitive(data, "app");
                    cJSON *title = cJSON_GetObjectItemCaseSensitive(data, "title");
                    cJSON *txt = cJSON_GetObjectItemCaseSensitive(data, "txt");
                    cJSON *ts = cJSON_GetObjectItemCaseSensitive(data, "ts");

                    if (cJSON_IsString(id))
                        strncpy(n_data.id, id->valuestring, sizeof(n_data.id) - 1);
                    if (cJSON_IsString(app))
                        strncpy(n_data.app, app->valuestring, sizeof(n_data.app) - 1);
                    if (cJSON_IsString(title))
                        strncpy(n_data.title, title->valuestring, sizeof(n_data.title) - 1);
                    if (cJSON_IsString(txt))
                        strncpy(n_data.txt, txt->valuestring, sizeof(n_data.txt) - 1);
                    if (cJSON_IsNumber(ts))
                        n_data.ts = ts->valueint;

                    cJSON *grp = cJSON_GetObjectItemCaseSensitive(data, "grp");
                    if (cJSON_IsString(grp))
                        strncpy(n_data.grp, grp->valuestring, sizeof(n_data.grp) - 1);

                    ESP_LOGI(TAG, "Новое уведомление: [%s] %s - %s", n_data.app, n_data.title, n_data.txt);

                    // Отправляем событие в LVGL
                    esp_event_post(APP_EVENTS, EVT_BLE_NOTIFY_RX, &n_data, sizeof(n_data), pdMS_TO_TICKS(100));
                }
            }

            // ==========================================
            // ОБРАБОТКА УДАЛЕНИЯ УВЕДОМЛЕНИЯ (n_del)
            // ==========================================
            else if (strcmp(type_item->valuestring, "n_del") == 0) {
                svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_LONG, LEVEL_MEDIUM, DURATION_LONG);
                cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "d");
                if (cJSON_IsObject(data)) {
                    notify_del_data del_data;
                    memset(&del_data, 0, sizeof(del_data));

                    cJSON *id = cJSON_GetObjectItemCaseSensitive(data, "id");
                    cJSON *grp = cJSON_GetObjectItemCaseSensitive(data, "grp");

                    memset(&del_data, 0, sizeof(del_data));

                    if (cJSON_IsString(id)) {
                        strncpy(del_data.id, id->valuestring, sizeof(del_data.id) - 1);
                        ESP_LOGI(TAG, "Удаление уведомления: %s", del_data.id);

                        // Отправляем событие удаления в LVGL
                        esp_event_post(APP_EVENTS, EVT_BLE_NOTIFY_DEL, &del_data, sizeof(del_data), pdMS_TO_TICKS(100));
                    }
                    if (cJSON_IsString(grp))
                        strncpy(del_data.grp, grp->valuestring, sizeof(del_data.grp) - 1);
                }
            }
        }

        // 4. Обязательно удаляем JSON из памяти!
        cJSON_Delete(root);
        return 0;
    }

    ESP_LOGW(TAG, "Попытка чтения характеристики уведомления");
    return BLE_ATT_ERR_UNLIKELY;
}

static int action_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    // Эта характеристика теперь только NOTIFY (Часы -> Телефон).
    // Телефон не может в нее писать!
    // Сюда будут приходить только запросы на подписку (чтение/запись дескриптора 0x2902),
    // но NimBLE обрабатывает 0x2902 автоматически под капотом.

    ESP_LOGW(TAG, "Попытка доступа к характеристике действия (только NOTIFY)");
    return BLE_ATT_ERR_UNLIKELY;
}

// Коллбэк (минимальный, т.к. телефон только подписывается)
static int log_chr_access(uint16_t conn_handle, uint16_t attr_handle, 
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    // NimBLE сам обрабатывает CCCD (0x2902), сюда попадут только невалидные запросы
    ESP_LOGD(TAG, "Доступ к log-характеристике (op=%d)", ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_UUID_TIME_SYNC),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_CHR_UUID_TIME_SYNC),
                    .access_cb = time_chr_access,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_CHR_UUID_NOTIFY),
                    .access_cb = notify_chr_access,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_CHR_UUID_ACTION),
                    .access_cb = action_chr_access,
                    .val_handle = &action_chr_val_handle,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_CHR_UUID_LOG_STREAM),
                    .access_cb = log_chr_access,
                    .val_handle = &log_chr_val_handle,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                },
                {0},  // ← Терминатор массива характеристик
            },
    },
    {0}  // ← Терминатор массива сервисов
};

static void gatt_svc_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {}

void send_action_to_phone(uint8_t action_code) {
    if (active_conn_handle == 0 || action_chr_val_handle == 0) {
        ESP_LOGW(TAG, "Нет подключения или хэндла");
        return;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(&action_code, 1);
    if (om == NULL)
        return;

    ble_gatts_notify_custom(active_conn_handle, action_chr_val_handle, om);
    ESP_LOGI(TAG, "Действие 0x%02X отправлено на телефон", action_code);
}

static const char *addr_type_to_str(uint8_t type) {
    switch (type) {
    case BLE_ADDR_PUBLIC:
        return "PUBLIC";
    case BLE_ADDR_RANDOM:
        return "RANDOM";
    // case BLE_ADDR_RPA: return "RPA";      // Random Private Address
    // case BLE_ADDR_NRPA: return "NRPA";    // Non-resolvable Private Address
    default:
        return "UNKNOWN";
    }
}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "Телефон подключен! conn_handle=%d status=%d", event->connect.conn_handle, event->connect.status);

        int8_t rssi = 0;
        ble_gap_conn_rssi(active_conn_handle, &rssi);
        ESP_LOGI(TAG, "📶 RSSI телефона при подключении: %d dBm", rssi);

        if (event->connect.status != 0) {
            // Подключение не удалось — стек сам вернёт нас в adv, делать ничего не нужно.
            ESP_LOGW(TAG, "Подключение не удалось (status=%d), перезапуск рекламы", event->connect.status);
            if (ble_running)
                ble_advertise();
            break;
        }

        if (event->connect.status == 0) {
            active_conn_handle = event->connect.conn_handle;
            uint16_t conn_handle = event->connect.conn_handle;
            active_conn_handle = conn_handle;

            struct ble_gap_upd_params conn_params = {
                .itvl_min = 0x30,  // 48 * 0.625 = 30 мс
                .itvl_max = 0x50,  // 80 * 0.625 = 50 мс
                .latency = 2,      // Пропускать 2 пакета (экономия энергии, phone tolerant)
                .supervision_timeout = 400, // 400 * 10 мс = 4 секунды (критично для Android!)
                .min_ce_len = 0x000A,
                .max_ce_len = 0x0020
            };

            int rc = ble_gap_update_params(conn_handle, &conn_params);
            if (rc == 0) {
                ESP_LOGI(TAG, "✓ Параметры соединения обновлены для стабильности (30-50ms, timeout=7.2s)");
            } else {
                ESP_LOGW(TAG, "⚠ Не удалось обновить параметры: %d (0x%X)", rc, rc);
            }

            // ★ НОВОЕ: Если ключи ещё не загружены — даём 100мс на завершение
            if (!ble_bonds_loaded) {
                ESP_LOGW(TAG, "⚠ Подключение до загрузки ключей, ждём 100мс...");
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            // Проверяем состояние шифрования
            struct ble_gap_conn_desc desc;
            if (ble_gap_conn_find(active_conn_handle, &desc) == 0) {
                if (desc.sec_state.encrypted) {
                    ESP_LOGI(TAG, "✓ Соединение уже зашифровано (bond восстановлен из NVS)");
                } else {
                    ESP_LOGI(TAG, "⏳ Соединение не зашифровано. Инициируем запрос на сопряжение...");
                    // Принудительно запускаем процесс сопряжения (вызовет BLE_GAP_EVENT_PASSKEY_ACTION)
                    int rc_sec = ble_gap_security_initiate(event->connect.conn_handle);
                    if (rc_sec != 0) {
                        ESP_LOGE(TAG, "Ошибка инициации безопасности: %d", rc_sec);
                    }
                }
            }
        }

        svc_vibro_send_event(VIBRATION_EVENT_SINGLE_SHORT, LEVEL_MAX, DURATION_DEFAULT);
        ble_advertising = false;
        active_conn_handle = event->connect.conn_handle;
        {
            struct ble_gap_conn_desc desc;
            if (ble_gap_conn_find(active_conn_handle, &desc) == 0 && desc.sec_state.encrypted) {
                ESP_LOGI(TAG, "✓ Соединение уже зашифровано (восстановлено из bonding)");
            } else {
                ESP_LOGI(TAG, "… Ожидаем, что central инициирует шифрование (LTK из bond)");
            }
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Телефон отключен.");
        active_conn_handle = 0;
        // Стираем PIN с экрана, если он там остался
        esp_event_post(APP_EVENTS, EVT_BLE_CLOSE_PIN, NULL, 0, pdMS_TO_TICKS(100));
        svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_LONG, LEVEL_HIGH, DURATION_DEFAULT);
        if (ble_running)
            ble_advertise();
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status == 0) {
            ESP_LOGI(TAG, "✓ Шифрование установлено успешно");
            esp_event_post(APP_EVENTS, EVT_BLE_CLOSE_PIN, NULL, 0, pdMS_TO_TICKS(100));
            svc_vibro_send_event(VIBRATION_EVENT_SINGLE_SHORT, LEVEL_MAX, DURATION_DEFAULT);
        } else {
            ESP_LOGE(TAG, "✗ Ошибка шифрования: 0x%04X (%d) — НЕ трогаем bond, ждём восстановления",
                     event->enc_change.status, event->enc_change.status);
        }
        break;
    
    case BLE_GAP_EVENT_SUBSCRIBE:
        // ★ ОТЛАДКА: покажем все параметры события
        ESP_LOGI(TAG, "📡 SUBSCRIBE: attr_handle=0x%04X, cur_notify=%d, cur_indicate=%d",
                event->subscribe.attr_handle,
                event->subscribe.cur_notify,
                event->subscribe.cur_indicate);
        
        // ★ FIX: сравниваем с хэндлом характеристики, а не дескриптора!
        if (event->subscribe.attr_handle == log_chr_val_handle) {  // ← БЫЛО: +1, СТАЛО: без +1
            log_subscribed = (event->subscribe.cur_notify != 0);
            ESP_LOGI(TAG, "📡 Log subscription: %s", 
                    log_subscribed ? "✅ ENABLED" : "❌ DISABLED");
            
            if (!log_subscribed) {
                // Очистка очереди при отписке
                ble_log_item_t *item;
                int count = 0;
                while (xQueueReceive(log_queue, &item, 0) == pdTRUE && item) {
                    free(item);
                    count++;
                }
                ESP_LOGI(TAG, "   🗑️ Очищено %d элементов из очереди", count);
            }
        }
        break;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI(TAG, "PASSKEY_ACTION event");
        // Если стек просит нас показать PIN-код (DISP)
        if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
            svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_SHORT, LEVEL_MAX, DURATION_DEFAULT);
            //if (svc_screen_sleep_is_sleeping()) {
            //    svc_screen_sleep_wake_up();
            //}
            struct ble_sm_io pkey = {0};

            // 1. Генерируем случайный 6-значный PIN (от 0 до 999999)
            uint32_t passkey_val = esp_random() % 1000000;

            // 2. Форматируем с ведущими нулями (например, 004231) для дисплея
            char pin_str[7];
            snprintf(pin_str, sizeof(pin_str), "%06lu", (unsigned long)passkey_val);
            ESP_LOGI(TAG, ">>> ВВЕДИТЕ PIN НА ТЕЛЕФОНЕ: %s <<<", pin_str);

            ble_pin_event_data_t evt_data;
            strncpy(evt_data.pin, pin_str, sizeof(evt_data.pin));
            evt_data.pin[sizeof(evt_data.pin) - 1] = '\0';
            
            // Отправляем событие. Последний аргумент - таймаут ожидания места в очереди событий
            esp_err_t err = esp_event_post(APP_EVENTS, EVT_BLE_SHOW_PIN, &evt_data, sizeof(evt_data), pdMS_TO_TICKS(100));
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to post PIN event: %s", esp_err_to_name(err));
            }

            // 4. Передаем сгенерированный PIN обратно в стек NimBLE!
            pkey.action = BLE_SM_IOACT_DISP;
            pkey.passkey = passkey_val;
            int rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            if (rc != 0) {
                ESP_LOGE(TAG, "ble_sm_inject_io failed: %d", rc);
            }
        }
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        struct ble_gap_conn_desc desc;
        int rc_rp = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc_rp == 0) {
            ESP_LOGW(TAG, "🔄 REPEAT_PAIRING: Телефон пытается пересопрячься (видимо, забыл устройство). Удаляем старый ключ и разрешаем новое сопряжение.");
            // Удаляем старый недействительный ключ для этого адреса
            ble_store_util_delete_peer(&desc.peer_id_addr);
        } else {
            ESP_LOGW(TAG, "🔄 REPEAT_PAIRING (desc lookup rc=%d) — разрешаем новое сопряжение", rc_rp);
        }
        // КРИТИЧНО: Возвращаем RETRY, а не IGNORE!
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    default:
        break;
    }
    return 0;
}

static void ble_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = 9; // ★ Явно указываем +9 dBm в пакете рекламы
    fields.name = (uint8_t *)"DharmaX";
    fields.name_len = strlen("DharmaX");
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) { ESP_LOGE(TAG, "adv_set_fields error: %d", rc); return; }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    
    // ★ ИНТЕРВАЛЫ РЕКЛАМЫ (в единицах 0.625 мс)
    // 0x60 = 96 * 0.625 = 60 мс (быстрое обнаружение, но не перегружает эфир)
    adv_params.itvl_min = 0x60; 
    adv_params.itvl_max = 0x80; // 100 мс
    //adv_params.channel_map = BLE_GAP_ADV_CHANNEL_MAP_ALL; // Все 3 канала (37,38,39)

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) { ESP_LOGE(TAG, "adv_start error: %d", rc); return; }

    ble_advertising = true;
    ESP_LOGI(TAG, "📡 Реклама запущена (60-100 мс, TX=+9 dBm)");
}

static void ble_on_sync(void) {
    int rc;
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ensure_addr error: %d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer_addr error: %d", rc);
        return;
    }

    int peer_count = 0;
    esp_err_t rc_count = ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC, &peer_count);
    if (rc_count == ESP_OK) {
        ESP_LOGI(TAG, "✅ Найдено сохранённых устройств (ключей) в NVS: %d", peer_count);
    } else {
        ESP_LOGW(TAG, "⚠ Не удалось получить количество ключей: %d", rc_count);
    }

    if (ble_running)
        ble_advertise();
}

static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task запущена");

    // ★ Ждём полной синхронизации стека перед рекламой!
    // nimble_port_run() сначала вызывает sync_cb, потом входит в цикл событий
    nimble_port_run();

    nimble_port_freertos_deinit();

    ble_running = false;
    ble_advertising = false;
    ESP_LOGI(TAG, "BLE Host Task завершена");
}

/**
 * @brief Инициализация с правильной последовательностью
 */
static void ble_do_init(void) {
    int rc;

    

    ble_initialized = true;
    ble_running = true; // Помечаем сразу, чтобы settings_tile знал

    // 1. NimBLE port init
    rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "   [1/7] nimble_port_init OK");

    ble_log_stream_init();
    

    // ★ Увеличиваем TX Power до +9 dBm (максимум)
    esp_err_t tx_err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    if (tx_err == ESP_OK) {
        ESP_LOGI(TAG, "✅ BLE TX Power set to +9 dBm (max)");
    } else {
        ESP_LOGW(TAG, "⚠ Failed to set TX Power: %s", esp_err_to_name(tx_err));
    } 

    // Для NimBLE также рекомендуется явно указать мощность для рекламы и соединения
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL0, ESP_PWR_LVL_P9);

    // 2. Безопасность (Включаем сопряжение с PIN-кодом)
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY; // У нас есть экран!
    ble_hs_cfg.sm_bonding = 1;                      // Запоминаем устройства
    ble_hs_cfg.sm_mitm = 1;                         // ВКЛЮЧАЕМ: требует MITM защиту (вызовет PIN)
    ble_hs_cfg.sm_sc = 1;                           // LE Secure Connections

    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | // LTK + EDIV/Rand (для шифрования)
                                 BLE_SM_PAIR_KEY_DIST_ID;   // IRK + BD_ADDR (для распознавания по RPA)

    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | // Принимаем ключи шифрования от central
                                   BLE_SM_PAIR_KEY_DIST_ID;   // Принимаем IRK для распознавания телефона

    ESP_LOGI(TAG, "✓ Настроен обмен ключами: ENC+ID (поддержка RPA)");

    // 3. Callbacks
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svc_register_cb;

    // 4. Имя устройства
    rc = ble_svc_gap_device_name_set("DharmaX");
    assert(rc == 0);

    // 5. GAP сервис
    ble_svc_gap_init();

    // 6. GATT сервисы
    rc = ble_gatts_count_cfg(gatt_services);
    assert(rc == 0);
    rc = ble_gatts_add_svcs(gatt_services);
    assert(rc == 0);

    // 7. Хранилище
    ble_store_config_init();

    vTaskDelay(pdMS_TO_TICKS(100));

    // Попробуем записать и прочитать тестовый ключ
    ESP_LOGI(TAG, "Тест записи в BLE store...");
    struct ble_store_value_sec test_sec;
    memset(&test_sec, 0, sizeof(test_sec));
    test_sec.peer_addr.type = BLE_ADDR_PUBLIC;
    memcpy(test_sec.peer_addr.val, "\x01\x02\x03\x04\x05\x06", 6);
    test_sec.ltk_present = 1;
    
    int rc_write = ble_store_write_peer_sec(&test_sec);
    ESP_LOGI(TAG, "  write rc=%d", rc_write);
    
    struct ble_store_key_sec key_sec;
    memset(&key_sec, 0, sizeof(key_sec));
    key_sec.peer_addr = test_sec.peer_addr;
    
    struct ble_store_value_sec read_sec;
    int rc_read = ble_store_read_peer_sec(&key_sec, &read_sec);
    ESP_LOGI(TAG, "  read rc=%d (0=OK)", rc_read);
    
    // Удаляем тестовый ключ
    ble_store_util_delete_peer(&test_sec.peer_addr);

    // Опционально: проверить, что ключи действительно загружены
    //int peer_count = ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC, NULL);
    //ESP_LOGI(TAG, "Найдено сохранённых устройств в NVS: %d", peer_count);
    ble_bonds_loaded = true; // Сигнализируем, что загрузка завершена

    // 8. Запуск задачи (реклама начнётся автоматически в ble_on_sync → ble_advertise)
    nimble_port_freertos_init(ble_host_task);

    // ble_advertise();
    ble_log_sender_start();

    ESP_LOGI(TAG, "🧪 TEST: обычный лог без скобок в тексте");
    ESP_LOGW(TAG, "🧪 TEST: лог с [квадратными] скобками в тексте");
    ESP_LOGE(TAG, "🧪 TEST: лог с (круглыми) и [квадратными] скобками");
    ESP_LOGI("CUSTOM_TAG", "🧪 TEST: кастомный тег");

    ESP_LOGI(TAG, "🚀 BLE инициализирован");

    // После ble_gatts_add_svcs():
    ESP_LOGI(TAG, "✅ Характеристики зарегистрированы:");
    ESP_LOGI(TAG, "   action: 0x%04X", action_chr_val_handle);
    ESP_LOGI(TAG, "   logs: 0x%04X", log_chr_val_handle);

    if (log_chr_val_handle == 0) {
        ESP_LOGE(TAG, "❌ КРИТИЧЕСКАЯ ОШИБКА: log_chr_val_handle = 0!");
        ESP_LOGE(TAG, "   Характеристика логов НЕ зарегистрирована!");
    }
}

// ==========================================
// PUBLIC API
// ==========================================

// ==========================================
// ФУНКЦИИ УПРАВЛЕНИЯ КЛЮЧАМИ
// ==========================================

/**
 * @brief Callback для итерации по хранилищу и удаления всех устройств
 */
static int delete_all_peers_iter_cb(int obj_type, union ble_store_value *val, void *cookie) {
    // Если это ключ безопасности устройства - удаляем его
    if (obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
        ble_store_util_delete_peer(&val->sec.peer_addr);
    }
    return 0; // Возвращаем 0, чтобы продолжить итерацию и удалить остальные ключи
}

/**
 * @brief Принудительно удалить ключи сопряжения для ТЕКУЩЕГО подключенного устройства
 */
void svc_ble_delete_current_peer(void) {
    if (active_conn_handle == 0) {
        ESP_LOGW(TAG, "Нет активного подключения, удалять нечего");
        return;
    }

    struct ble_gap_conn_desc desc;
    int rc = ble_gap_conn_find(active_conn_handle, &desc);
    if (rc == 0) {
        ESP_LOGI(TAG, "Принудительное удаление ключей для текущего устройства");
        ble_store_util_delete_peer(&desc.peer_id_addr);

        // Разрываем соединение, чтобы телефон понял, что ключи стерты
        ble_gap_terminate(active_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        active_conn_handle = 0;
    } else {
        ESP_LOGE(TAG, "Не найти дескриптор подключения: %d", rc);
    }
}

/**
 * @brief Принудительно удалить ВСЕ ключи сопряжения из NVS (Flash)
 * Используйте это для функции "Забыть все устройства" в настройках часов
 */
void svc_ble_delete_all_peers(void) {
    ESP_LOGI(TAG, "Принудительное удаление ВСЕХ ключей из памяти!");

    // 1. Сначала отключаем телефон, если он подключен
    if (active_conn_handle != 0) {
        ble_gap_terminate(active_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        active_conn_handle = 0;
        vTaskDelay(pdMS_TO_TICKS(100)); // Даем время на разрыв
    }

    // 2. Итерируемся по всем сохраненным ключам безопасности и удаляем их
    // Это заменяет отсутствующую функцию ble_store_config_delete_all
    ble_store_iterate(BLE_STORE_OBJ_TYPE_PEER_SEC, delete_all_peers_iter_cb, NULL);

    ESP_LOGI(TAG, "Все ключи удалены. При следующем подключении потребуется PIN.");

    // 3. Перезапускаем рекламу, чтобы можно было подключиться заново
    if (ble_running && !ble_advertising) {
        ble_advertise();
    }
}

void svc_ble_sync_stop(void) {
    if (!ble_running)
        return;

    ESP_LOGI(TAG, "◄ Остановка BLE...");

    // Только останавливаем рекламу и помечаем что не работаем
    if (ble_advertising) {
        ble_gap_adv_stop();
        ble_advertising = false;
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (active_conn_handle != 0) {
        ble_gap_terminate(active_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        active_conn_handle = 0; // Сбрасываем
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    // ★ НЕ трогаем nimble_port! Он продолжает работать в фоне.
    // ★ Это потребляет ~1-2mA но ЗАТО гарантированно работает!

    ble_running = false;
    ESP_LOGI(TAG, "BLE остановлен (режим ожидания, экономия ~3mA)");
}

void svc_ble_sync_start(void) {
    if (ble_running) {
        ESP_LOGW(TAG, "BLE уже работает");
        return;
    }

    // Если NimBLE ещё не инициализирован - первый запуск
    if (!ble_initialized) {
        ble_do_init();
    } else {
        // Повторный запуск - просто запускаем рекламу снова!
        ble_running = true;
        ble_advertise(); // ← Всё! Никакой reinit!
        ESP_LOGI(TAG, "▶️ BLE возобновлён (быстрый старт)");
    }
}

bool svc_ble_sync_is_running(void) { return ble_running; }

void svc_ble_sync_advertise_stop(void) {
    if (!ble_running || !ble_advertising)
        return;

    int rc = ble_gap_adv_stop();
    if (rc == 0) {
        ble_advertising = false;
        ESP_LOGI(TAG, "Реклама остановлена (стек активен)");
    }
}

void svc_ble_sync_advertise_start(void) {
    if (!ble_running || ble_advertising)
        return;
    ble_advertise();
}

bool svc_ble_sync_is_advertising(void) { return ble_advertising; }