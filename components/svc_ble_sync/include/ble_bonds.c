#include "ble_bonds.h"
#include "ble_advertise.h"
#include "ble_state.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"

static const char *TAG = "BLE_BONDS";

static int delete_all_peers_iter_cb(int obj_type, union ble_store_value *val, void *cookie) {
    if (obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
        ble_store_util_delete_peer(&val->sec.peer_addr);
    }
    return 0; // продолжаем итерацию
}

void svc_ble_delete_current_peer(void) {
    if (ble_state.active_conn_handle == 0) {
        ESP_LOGW(TAG, "Нет активного подключения, удалять нечего");
        return;
    }

    struct ble_gap_conn_desc desc;
    int rc = ble_gap_conn_find(ble_state.active_conn_handle, &desc);
    if (rc != 0) {
        ESP_LOGE(TAG, "Не удалось найти дескриптор подключения: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Принудительное удаление ключей для текущего устройства");
    ble_store_util_delete_peer(&desc.peer_id_addr);

    // Разрываем соединение, чтобы телефон понял, что ключи стёрты
    ble_gap_terminate(ble_state.active_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    ble_state.active_conn_handle = 0;
}

void svc_ble_delete_all_peers(void) {
    ESP_LOGI(TAG, "Принудительное удаление ВСЕХ ключей из памяти!");

    if (ble_state.active_conn_handle != 0) {
        ble_gap_terminate(ble_state.active_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        ble_state.active_conn_handle = 0;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ble_store_iterate(BLE_STORE_OBJ_TYPE_PEER_SEC, delete_all_peers_iter_cb, NULL);
    ESP_LOGI(TAG, "Все ключи удалены. При следующем подключении потребуется PIN.");

    if (ble_state.running && !ble_state.advertising) {
        ble_advertise_start();
    }
}

void ble_bonds_selftest(void) {
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

    ble_store_util_delete_peer(&test_sec.peer_addr);
}