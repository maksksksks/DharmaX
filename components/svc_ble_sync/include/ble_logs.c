#include "ble_logs.h"
#include "ble_state.h"

static const char *TAG = "BLE_LOGS";

/**
 * @brief Парсит строку лога в формате ESP-IDF: "I (1234) TAG: message"
 * @return true если успешно распаршено
 */
// Возвращает: 1 = распарсено успешно, 0 = не распарсилось, -1 = отфильтровано (пропустить)
int parse_esp_log_line(const char *full_msg, ble_log_item_t *item) {
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
    if (ble_state.original_vprintf) {
        ret = ble_state.original_vprintf(fmt, args);
    }

    // ★ ОТЛАДКА: покажем, что хук вызван
    static int hook_call_count = 0;
    hook_call_count++;
    if (hook_call_count <= 10) {  // Первые 10 вызовов
        printf("[HOOK #%d] ble_state.running=%d, ble_state.log_subscribed=%d, queue=%p\n", 
               hook_call_count, ble_state.running, ble_state.log_subscribed, ble_state.log_queue);
    }

    // 3. Проверки
    if (!ble_state.running) {
        if (hook_call_count <= 5) {
            printf("[HOOK] SKIP: ble_state.running=false\n");
        }
        return ret;
    }
    if (!ble_state.log_subscribed) {
        if (hook_call_count <= 5) {
            printf("[HOOK] SKIP: ble_state.log_subscribed=false\n");
        }
        return ret;
    }
    if (ble_state.log_queue == NULL) {
        if (hook_call_count <= 5) {
            printf("[HOOK] SKIP: ble_state.log_queue=NULL\n");
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
    if (xQueueIsQueueFullFromISR(ble_state.log_queue)) {
        ble_log_item_t *dummy;
        xQueueReceive(ble_state.log_queue, &dummy, 0);
        if (dummy) free(dummy);
    }

    if (xQueueSend(ble_state.log_queue, &item, 0) != pdTRUE) {
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
        if (xQueueReceive(ble_state.log_queue, &item, pdMS_TO_TICKS(50)) == pdTRUE && item) {
            
            // Проверяем актуальность подключения
            if (!ble_state.log_subscribed || ble_state.active_conn_handle == 0) {
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
                int rc = ble_gatts_notify_custom(ble_state.active_conn_handle, 
                                                 ble_state.log_chr_val_handle, om);
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
    ble_state.log_queue = xQueueCreate(20, sizeof(ble_log_item_t*));
    if (!ble_state.log_queue) {
        ESP_LOGE(TAG, "❌ Failed to create log queue!");
        return;
    }
    ESP_LOGI(TAG, "✅ Log queue created");

    ble_state.original_vprintf = esp_log_set_vprintf(ble_log_hook_vprintf);
    if (ble_state.original_vprintf == NULL) {
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