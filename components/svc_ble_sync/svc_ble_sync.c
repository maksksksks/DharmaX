#include "svc_ble_sync.h"
#include "app_events.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "esp_random.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "lvgl_ui.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "svc_screen_sleep.h"
#include "svc_vibro.h"
#include "sys/time.h"

void ble_store_config_init(void);

static const char *TAG = "SVC_BLE";

#define BLE_SVC_UUID_TIME_SYNC 0x1234
#define BLE_CHR_UUID_TIME_SYNC 0x5678
#define BLE_CHR_UUID_NOTIFY    0x5679 // Телефон пишет текст сюда
#define BLE_CHR_UUID_ACTION    0x5680 // Часы шлют кнопки отсюда

static uint16_t action_chr_val_handle = 0; // Хэндл характеристики
static uint16_t active_conn_handle = 0;    // Хэндл подключения
static uint16_t time_sync_conn_handle;
static uint8_t own_addr_type;

// Состояние
static bool ble_running = false;
static bool ble_advertising = false;
static bool ble_initialized = false;

static void ble_advertise(void);

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

// Функция для разделения строки "Telegram: Текст"
static notify_data parse_notify_string(const char* raw) {
    notify_data out;
    memset(&out, 0, sizeof(out));
    
    // Ищем ": " в строке
    char *separator = strstr(raw, ": ");
    
    if (separator != NULL) {
        // Копируем приложение (до двоеточия)
        int app_len = separator - raw;
        if (app_len >= 50) app_len = 49;
        strncpy(out.app, raw, app_len);
        
        // Копируем данные (после двоеточия и пробела)
        strncpy(out.data, separator + 2, 256);
    } else {
        // Если формат неправильный, кидаем всё в данные
        strncpy(out.data, raw, 256);
    }
    
    out.time_life = 600; // 10 минут (для справки)
    return out;
}

static int notify_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_LONG, LEVEL_HIGH, DURATION_LONG);
        
        uint16_t len = ctxt->om->om_len;
        if (len > 256) len = 256; 
        
        char buf[257]; 
        memcpy(buf, ctxt->om->om_data, len);
        buf[len] = '\0'; 

        ESP_LOGI(TAG, "Получено уведомление (%d байт): %s", len, buf);
        
        // ★ Парсим строку в структуру
        notify_data n_data = parse_notify_string(buf);
        
        // ★ Отправляем событие в главный поток LVGL!
        // Используем esp_event_post, так как он копирует данные (n_data) в очередь,
        // и нам не страшно, что локальная переменная удалится после выхода из коллбэка.
        esp_event_post(APP_EVENTS, EVT_BLE_NOTIFY_RX, &n_data, sizeof(n_data), pdMS_TO_TICKS(100));
        
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

static const struct ble_gatt_svc_def gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_UUID_TIME_SYNC),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(BLE_CHR_UUID_TIME_SYNC),
                .access_cb = time_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_AUTHEN,
            },
            {
                // Телефон отправляет текст на часы
                .uuid = BLE_UUID16_DECLARE(BLE_CHR_UUID_NOTIFY),
                .access_cb = notify_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_AUTHEN,
            },
            {
                // Часы отправляют кнопки на телефон
                .uuid = BLE_UUID16_DECLARE(BLE_CHR_UUID_ACTION),
                .access_cb = action_chr_access,
                .val_handle = &action_chr_val_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY, // ★ ТОЛЬКО NOTIFY!
            },
            {0}
        },
    },
    {0}
};

static void gatt_svc_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {}

// Вызывай эту функцию при нажатии кнопки на часах
void send_action_to_phone(uint8_t action_code) {
    if (active_conn_handle == 0 || action_chr_val_handle == 0) {
        ESP_LOGW(TAG, "Нет подключения или хэндла");
        return;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(&action_code, 1);
    if (om == NULL) return;

    ble_gatts_notify_custom(active_conn_handle, action_chr_val_handle, om);
    ESP_LOGI(TAG, "Действие 0x%02X отправлено на телефон", action_code);
}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "Телефон подключен! Ожидаем сопряжения...");
        svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_SHORT, LEVEL_MAX, DURATION_DEFAULT);
        ble_advertising = false;
        active_conn_handle = event->connect.conn_handle;

        int rc = ble_gap_security_initiate(event->connect.conn_handle);
        if (rc != 0) {
            ESP_LOGE(TAG, "Ошибка инициализации безопасности: %d", rc);
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Телефон отключен.");
        // Стираем PIN с экрана, если он там остался
        close_pairing_popup_cb(NULL);
        svc_vibro_send_event(VIBRATION_EVENT_SINGLE_LONG, LEVEL_HIGH, DURATION_DEFAULT);
        if (ble_running)
            ble_advertise();
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        // Шифрование установлено! Сопряжение прошло успешно.
        if (event->enc_change.status == 0) {
            ESP_LOGI(TAG, "Шифрование установлено! Авторизовано.");
            // PIN больше не нужен, очищаем экран
            close_pairing_popup_cb(NULL);
            svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_SHORT, LEVEL_MAX, DURATION_DEFAULT);
        } else {
            ESP_LOGE(TAG, "Ошибка шифрования: %d", event->enc_change.status);
            // display_clear_ble_pin();
        }
        break;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI(TAG, "PASSKEY_ACTION event");
        // Если стек просит нас показать PIN-код (DISP)
        if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
            svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_SHORT, LEVEL_MAX, DURATION_DEFAULT);
            if (svc_screen_sleep_is_sleeping()) {
                svc_screen_sleep_wake_up();
            }
            struct ble_sm_io pkey = {0};

            // 1. Генерируем случайный 6-значный PIN (от 0 до 999999)
            uint32_t passkey_val = esp_random() % 1000000;

            // 2. Форматируем с ведущими нулями (например, 004231) для дисплея
            char pin_str[7];
            snprintf(pin_str, sizeof(pin_str), "%06lu", (unsigned long)passkey_val);
            ESP_LOGI(TAG, ">>> ВВЕДИТЕ PIN НА ТЕЛЕФОНЕ: %s <<<", pin_str);

            // 3. Выводим PIN на экран часов (раскомментируй/адаптируй под свой дисплей)
            show_pairing_popup(pin_str);

            // 4. Передаем сгенерированный PIN обратно в стек NimBLE!
            pkey.action = BLE_SM_IOACT_DISP;
            pkey.passkey = passkey_val;
            int rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            if (rc != 0) {
                ESP_LOGE(TAG, "ble_sm_inject_io failed: %d", rc);
            }
        }
        break;

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
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t *)"DharmaX";
    fields.name_len = strlen("DharmaX");
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields error: %d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    adv_params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start error: %d", rc);
        return;
    }

    ble_advertising = true;
    ESP_LOGI(TAG, "Реклама запущена.");
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

    // 1. NimBLE port init
    rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "   [1/7] nimble_port_init OK");

    // 2. Безопасность (Включаем сопряжение с PIN-кодом)
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY; // У нас есть экран!
    ble_hs_cfg.sm_bonding = 1;                      // Запоминаем устройства
    ble_hs_cfg.sm_mitm = 1;                         // ВКЛЮЧАЕМ: требует MITM защиту (вызовет PIN)
    ble_hs_cfg.sm_sc = 1;                           // LE Secure Connections

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

    // 8. Запуск задачи (реклама начнётся автоматически в ble_on_sync → ble_advertise)
    nimble_port_freertos_init(ble_host_task);

    ble_initialized = true;
    ble_running = true; // Помечаем сразу, чтобы settings_tile знал

    ble_advertise();

    ESP_LOGI(TAG, "🚀 BLE инициализирован");
}

// ==========================================
// PUBLIC API
// ==========================================

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

    ble_gap_terminate(0, BLE_ERR_REM_USER_CONN_TERM);
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