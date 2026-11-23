#include "ble_gap_handler.h"
#include "ble_advertise.h"
#include "ble_logs.h"
#include "ble_state.h"

#include <stdlib.h>
#include <string.h>

#include "app_events.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "svc_vibro.h"

static const char *TAG = "BLE_GAP";

// ==========================================
// CONNECT
// ==========================================

static void on_connect(struct ble_gap_event *event) {
    const uint16_t conn_handle = event->connect.conn_handle;

    ESP_LOGI(TAG, "Телефон подключен! conn_handle=%d status=%d", conn_handle, event->connect.status);

    if (event->connect.status != 0) {
        ESP_LOGW(TAG, "Подключение не удалось (status=%d), перезапуск рекламы", event->connect.status);
        if (ble_state.running) ble_advertise_start();
        return;
    }

    ble_state.active_conn_handle = conn_handle;
    ble_state.advertising = false;

    int8_t rssi = 0;
    if (ble_gap_conn_rssi(conn_handle, &rssi) == 0) {
        ESP_LOGI(TAG, "📶 RSSI телефона при подключении: %d dBm", rssi);
    }

    // Интервал соединения измеряется в 1.25 мс, timeout — в 10 мс
    struct ble_gap_upd_params conn_params = {
        .itvl_min = 0x30,            // 48 * 1.25 = 60 мс
        .itvl_max = 0x50,            // 80 * 1.25 = 100 мс
        .latency = 2,
        .supervision_timeout = 400,  // 400 * 10 = 4 с
        .min_ce_len = 0x000A,
        .max_ce_len = 0x0020,
    };
    int rc = ble_gap_update_params(conn_handle, &conn_params);
    if (rc == 0) {
        ESP_LOGI(TAG, "✓ Параметры соединения обновлены (60-100 мс, latency=2, timeout=4 с)");
    } else {
        ESP_LOGW(TAG, "⚠ Не удалось обновить параметры: %d (0x%X)", rc, rc);
    }

    if (!ble_state.bonds_loaded) {
        ESP_LOGW(TAG, "⚠ Подключение до загрузки ключей, ждём 100мс...");
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(conn_handle, &desc) == 0) {
        if (desc.sec_state.encrypted) {
            ESP_LOGI(TAG, "✓ Соединение уже зашифровано (bond восстановлен из NVS)");
        } else {
            ESP_LOGI(TAG, "⏳ Соединение не зашифровано. Инициируем запрос на сопряжение...");
            int rc_sec = ble_gap_security_initiate(conn_handle);
            if (rc_sec != 0) {
                ESP_LOGE(TAG, "Ошибка инициации безопасности: %d", rc_sec);
            }
        }
    }

    svc_vibro_send_event(VIBRATION_EVENT_SINGLE_SHORT, LEVEL_MAX, DURATION_DEFAULT);
}

// ==========================================
// DISCONNECT
// ==========================================

static void on_disconnect(void) {
    ESP_LOGI(TAG, "Телефон отключен.");
    ble_state.active_conn_handle = 0;
    ble_state.log_subscribed = false;

    esp_event_post(APP_EVENTS, EVT_BLE_CLOSE_PIN, NULL, 0, pdMS_TO_TICKS(100));
    svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_LONG, LEVEL_HIGH, DURATION_DEFAULT);

    if (ble_state.running) ble_advertise_start();
}

// ==========================================
// ENC_CHANGE
// ==========================================

static void on_enc_change(struct ble_gap_event *event) {
    if (event->enc_change.status == 0) {
        ESP_LOGI(TAG, "✓ Шифрование установлено успешно");
        esp_event_post(APP_EVENTS, EVT_BLE_CLOSE_PIN, NULL, 0, pdMS_TO_TICKS(100));
        svc_vibro_send_event(VIBRATION_EVENT_SINGLE_SHORT, LEVEL_MAX, DURATION_DEFAULT);
    } else {
        ESP_LOGE(TAG, "✗ Ошибка шифрования: 0x%04X (%d) — НЕ трогаем bond, ждём восстановления",
                 event->enc_change.status, event->enc_change.status);
    }
}

// ==========================================
// SUBSCRIBE
// ==========================================

static void on_subscribe(struct ble_gap_event *event) {
    ESP_LOGI(TAG, "📡 SUBSCRIBE: attr_handle=0x%04X, cur_notify=%d, cur_indicate=%d",
             event->subscribe.attr_handle,
             event->subscribe.cur_notify,
             event->subscribe.cur_indicate);

    // attr_handle в событии — это handle значения характеристики
    if (event->subscribe.attr_handle != ble_state.log_chr_val_handle) return;

    ble_state.log_subscribed = (event->subscribe.cur_notify != 0);
    ESP_LOGI(TAG, "📡 Log subscription: %s", ble_state.log_subscribed ? "✅ ENABLED" : "❌ DISABLED");

    if (!ble_state.log_subscribed && ble_state.log_queue != NULL) {
        ble_log_item_t *item;
        int count = 0;
        while (xQueueReceive(ble_state.log_queue, &item, 0) == pdTRUE) {
            if (item) free(item);
            count++;
        }
        ESP_LOGI(TAG, "   🗑️ Очищено %d элементов из очереди", count);
    }
}

// ==========================================
// PASSKEY
// ==========================================

static void on_passkey(struct ble_gap_event *event) {
    ESP_LOGI(TAG, "PASSKEY_ACTION event");

    if (event->passkey.params.action != BLE_SM_IOACT_DISP) return;

    svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_SHORT, LEVEL_MAX, DURATION_DEFAULT);

    // Случайный 6-значный PIN с ведущими нулями
    uint32_t passkey_val = esp_random() % 1000000;
    char pin_str[7];
    snprintf(pin_str, sizeof(pin_str), "%06lu", (unsigned long)passkey_val);
    ESP_LOGI(TAG, ">>> ВВЕДИТЕ PIN НА ТЕЛЕФОНЕ: %s <<<", pin_str);

    ble_pin_event_data_t evt_data;
    strncpy(evt_data.pin, pin_str, sizeof(evt_data.pin));
    evt_data.pin[sizeof(evt_data.pin) - 1] = '\0';

    esp_err_t err = esp_event_post(APP_EVENTS, EVT_BLE_SHOW_PIN, &evt_data, sizeof(evt_data), pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post PIN event: %s", esp_err_to_name(err));
    }

    struct ble_sm_io pkey = {0};
    pkey.action = BLE_SM_IOACT_DISP;
    pkey.passkey = passkey_val;
    int rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_sm_inject_io failed: %d", rc);
    }
}

// ==========================================
// REPEAT_PAIRING
// ==========================================

static int on_repeat_pairing(struct ble_gap_event *event) {
    struct ble_gap_conn_desc desc;
    int rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
    if (rc == 0) {
        ESP_LOGW(TAG, "🔄 REPEAT_PAIRING: телефон пересопрягается. Удаляем старый ключ и разрешаем новое сопряжение.");
        ble_store_util_delete_peer(&desc.peer_id_addr);
    } else {
        ESP_LOGW(TAG, "🔄 REPEAT_PAIRING (desc lookup rc=%d) — разрешаем новое сопряжение", rc);
    }
    return BLE_GAP_REPEAT_PAIRING_RETRY; // не IGNORE!
}

// ==========================================
// Диспетчер
// ==========================================

int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:        on_connect(event);      break;
    case BLE_GAP_EVENT_DISCONNECT:     on_disconnect();        break;
    case BLE_GAP_EVENT_ENC_CHANGE:     on_enc_change(event);   break;
    case BLE_GAP_EVENT_SUBSCRIBE:      on_subscribe(event);    break;
    case BLE_GAP_EVENT_PASSKEY_ACTION: on_passkey(event);      break;
    case BLE_GAP_EVENT_REPEAT_PAIRING: return on_repeat_pairing(event);
    default: break;
    }
    return 0;
}