#include "ble_advertise.h"
#include "ble_state.h"
#include "ble_power.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "BLE_ADV";

static void adv_begin(bool fast) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = 3;   // соответствует ESP_PWR_LVL_P3 в ble_config.c
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

    adv_params.itvl_min = fast ? BLE_ADV_FAST_MIN : BLE_ADV_SLOW_MIN;
    adv_params.itvl_max = fast ? BLE_ADV_FAST_MAX : BLE_ADV_SLOW_MAX;

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
    ESP_LOGI(TAG, "Реклама запущена (%s)", fast ? "быстрая 60-80 мс" : "медленная 1.0-1.2 с");
}

// Публичный старт: быстрая реклама, затем по таймеру - медленная.
void ble_advertise_start(void) {
    adv_begin(true);
    if (ble_state.advertising) ble_power_adv_window_arm();
}

// Вызывается таймером по окончании окна быстрой рекламы.
void ble_advertise_restart_slow(void) {
    if (ble_state.advertising) {
        if (ble_gap_adv_stop() != 0) return;
        ble_state.advertising = false;
    }
    adv_begin(false);
}

void ble_advertise_stop(void) {
    ble_power_adv_window_cancel();
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