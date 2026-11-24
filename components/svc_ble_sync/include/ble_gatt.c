#include "ble_gatt.h"
#include "ble_state.h"
#include "ble_power.h"
#include "ble_notify_parser.h"
#include "svc_ble_sync.h"

#include <string.h>
#include <sys/time.h>

#include "app_events.h"
#include "esp_event.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "svc_vibro.h"

static const char *TAG = "BLE_GATT";

#define BLE_SVC_UUID_TIME_SYNC  0x1234
#define BLE_CHR_UUID_TIME_SYNC  0x5678
#define BLE_CHR_UUID_NOTIFY     0x5679 // Телефон пишет JSON сюда
#define BLE_CHR_UUID_ACTION     0x5680 // Часы шлют кнопки отсюда (NOTIFY)
#define BLE_CHR_UUID_LOG_STREAM 0x5681 // Поток логов (NOTIFY)

#define NOTIFY_MAX_LEN 512

// ==========================================
// Коллбэки характеристик
// ==========================================

static int time_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    ble_power_activity();

    if (OS_MBUF_PKTLEN(ctxt->om) != 4) {
        ESP_LOGE(TAG, "Неверный размер пакета времени!");
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint32_t epoch_time;
    if (os_mbuf_copydata(ctxt->om, 0, 4, &epoch_time) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    ESP_LOGI(TAG, "Получено время от телефона: %lu", (unsigned long)epoch_time);

    struct timeval tv = {.tv_sec = epoch_time, .tv_usec = 0};
    settimeofday(&tv, NULL);
    esp_event_post(APP_EVENTS, EVT_BLE_TIME_SYNCED, NULL, 0, pdMS_TO_TICKS(100));
    svc_vibro_send_event(VIBRATION_EVENT_SINGLE_SHORT, LEVEL_MEDIUM, DURATION_DEFAULT);
    return 0;
}

static int notify_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGW(TAG, "Попытка чтения характеристики уведомления");
        return BLE_ATT_ERR_UNLIKELY;
    }
    ble_power_activity();   // телефон слал данные - вероятно, будет ещё (длинные JSON, пачка уведомлений)

    // Безопасно копируем данные из mbuf (учитывая фрагментацию NimBLE)
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len > NOTIFY_MAX_LEN) len = NOTIFY_MAX_LEN;

    char buf[NOTIFY_MAX_LEN + 1];
    if (os_mbuf_copydata(ctxt->om, 0, len, buf) != 0) {
        ESP_LOGE(TAG, "Ошибка копирования mbuf");
        return BLE_ATT_ERR_UNLIKELY;
    }
    buf[len] = '\0';

    ESP_LOGI(TAG, "Получено JSON (%d байт): %s", len, buf);
    ble_notify_parse_json(buf);
    return 0; // Кривой пакет просто игнорируем
}

// Только NOTIFY (часы -> телефон). CCCD обрабатывает NimBLE сам,
// сюда попадут только невалидные обращения.
static int action_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGW(TAG, "Попытка доступа к характеристике действия (только NOTIFY)");
    return BLE_ATT_ERR_UNLIKELY;
}

static int log_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGD(TAG, "Доступ к log-характеристике (op=%d)", ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}

// ==========================================
// GATT-таблица
// ==========================================

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
                    .val_handle = &ble_state.action_chr_val_handle,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_CHR_UUID_LOG_STREAM),
                    .access_cb = log_chr_access,
                    .val_handle = &ble_state.log_chr_val_handle,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                },
                {0}, // терминатор характеристик
            },
    },
    {0} // терминатор сервисов
};

int ble_gatt_init(void) {
    int rc = ble_gatts_count_cfg(gatt_services);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return rc;
    }
    rc = ble_gatts_add_svcs(gatt_services);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
    }
    return rc;
}

// ==========================================
// Отправка действия на телефон
// ==========================================

void send_action_to_phone(uint8_t action_code) {
    if (ble_state.active_conn_handle == 0 || ble_state.action_chr_val_handle == 0) {
        ESP_LOGW(TAG, "Нет подключения или хэндла");
        return;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(&action_code, 1);
    if (om == NULL) return;

    ble_power_activity();   // ждём ответ телефона на действие - быстрый профиль

    ble_gatts_notify_custom(ble_state.active_conn_handle, ble_state.action_chr_val_handle, om);
    ESP_LOGI(TAG, "Действие 0x%02X отправлено на телефон", action_code);
}