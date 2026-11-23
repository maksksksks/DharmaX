#include "ble_config.h"

#include "esp_bt.h"
#include "esp_log.h"
#include "host/ble_hs.h"

static const char *TAG = "BLE_CFG";

void ble_config_security(void) {
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY; // есть экран, нет ввода
    ble_hs_cfg.sm_bonding = 1;                      // запоминаем устройства
    ble_hs_cfg.sm_mitm = 1;                         // MITM-защита (PIN)
    ble_hs_cfg.sm_sc = 1;                           // LE Secure Connections

    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ESP_LOGI(TAG, "✓ Настроен обмен ключами: ENC+ID (поддержка RPA)");
}

void ble_config_tx_power(void) {
    esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P3);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✅ BLE TX Power set to +3 dBm");
    } else {
        ESP_LOGW(TAG, "⚠ Failed to set TX Power: %s", esp_err_to_name(err));
    }
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P3);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL0, ESP_PWR_LVL_P3);
}