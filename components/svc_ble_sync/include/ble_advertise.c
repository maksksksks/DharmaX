#include "ble_advertise.h"
#include "ble_state.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "BLE_ADV";

void ble_advertise_start(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = 9;
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

    adv_params.itvl_min = 0x60;
    adv_params.itvl_max = 0x80;

    rc = ble_gap_adv_start(
        ble_state.own_addr_type,
        NULL,
        BLE_HS_FOREVER,
        &adv_params,
        ble_gap_event,
        NULL
    );

    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start error: %d", rc);
        return;
    }

    ble_state.advertising = true;
    ESP_LOGI(TAG, "Реклама запущена (60-100 мс, TX=+9 dBm)");
}

void ble_advertise_stop(void) {
    if (!ble_state.advertising)
        return;

    int rc = ble_gap_adv_stop();

    if (rc == 0) {
        ble_state.advertising = false;
        ESP_LOGI(TAG, "Реклама остановлена");
    } else {
        ESP_LOGE(TAG, "adv_stop error: %d", rc);
    }
}