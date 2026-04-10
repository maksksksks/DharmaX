#include "svc_ble_sync.h"
#include "app_events.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "sys/time.h"

// МАГИЯ ИЗ РАБОЧЕГО ПРИМЕРА: Объявляем функцию хранилища, чтобы компилятор не ругался
void ble_store_config_init(void);

static const char *TAG = "SVC_BLE";

#define BLE_SVC_UUID_TIME_SYNC       0x1234
#define BLE_CHR_UUID_TIME_SYNC       0x5678

static uint16_t time_sync_conn_handle;
static uint8_t own_addr_type;

static void ble_advertise(void);

static int time_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len == 4) {
            uint32_t epoch_time;
            memcpy(&epoch_time, ctxt->om->om_data, 4);
            
            ESP_LOGI(TAG, "Получено время от телефона: %lu", epoch_time);

            struct timeval tv = {
                .tv_sec = epoch_time,
                .tv_usec = 0
            };
            settimeofday(&tv, NULL);

            esp_event_post(APP_EVENTS, EVT_BLE_TIME_SYNCED, NULL, 0, pdMS_TO_TICKS(100));
            return 0;
        } else {
            ESP_LOGE(TAG, "Неверный размер пакета! Ожидалось 4 байта.");
            return BLE_ATT_ERR_UNLIKELY;
        }
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_UUID_TIME_SYNC),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(BLE_CHR_UUID_TIME_SYNC),
                .access_cb = time_chr_access,
                .val_handle = &time_sync_conn_handle,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 }
        },
    },
    { 0 }
};

// ИСПРАВЛЕНО: Должно быть void, а не int!
static void gatt_svc_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    // Пустая заглушка, как в примерах Espressif
}

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "Телефон подключен!");
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Телефон отключен. Перезапуск рекламы...");
            ble_advertise();
            break;
        default:
            break;
    }
    return 0;
}

static void ble_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof fields);
    // Добавляем флаги из рабочего примера
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t *)"WristWatch";
    fields.name_len = strlen("WristWatch");
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    // Обязательно задаем интервалы!
    adv_params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN; 
    adv_params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;

    // Сигнатура из рабочего примера (с NULL вместо params во второй позиции)
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
    
    ESP_LOGI(TAG, "Реклама запущена.");
}

static void ble_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "error ensuring address; rc=%d\n", rc);
        return;
    }

    // Вычисление типа адреса из рабочего примера
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    ble_advertise();
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task запущена");
    nimble_port_run(); 
    nimble_port_freertos_deinit();
}

void svc_ble_sync_start(void)
{
    int rc;

    rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble %d ", rc);
        return;
    }

    // === ДОБАВИТЬ ЭТО: ЖЕСТКИЙ ЗАПРЕТ НА СОПРЯЖЕНИЕ ===
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO; 
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;
    // ====================================================

    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svc_register_cb;

    rc = ble_svc_gap_device_name_set("WristWatch");
    assert(rc == 0);
    
    ble_svc_gap_init();
    
    rc = ble_gatts_count_cfg(gatt_services);
    assert(rc == 0);
    
    rc = ble_gatts_add_svcs(gatt_services);
    assert(rc == 0);

    ble_store_config_init();
    nimble_port_freertos_init(ble_host_task);
}