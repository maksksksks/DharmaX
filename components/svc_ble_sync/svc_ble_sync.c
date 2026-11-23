#include "svc_ble_sync.h"
#include "ble_advertise.h"
#include "ble_bonds.h"
#include "ble_config.h"
#include "ble_gatt.h"
#include "ble_logs.h"
#include "ble_state.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"

void ble_store_config_init(void);

static const char *TAG = "SVC_BLE";

// ==========================================
// Синхронизация стека и host-задача
// ==========================================

static void ble_on_sync(void) {
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ensure_addr error: %d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &ble_state.own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer_addr error: %d", rc);
        return;
    }

    int peer_count = 0;
    rc = ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC, &peer_count);
    if (rc == 0) {
        ESP_LOGI(TAG, "✅ Найдено сохранённых устройств (ключей) в NVS: %d", peer_count);
    } else {
        ESP_LOGW(TAG, "⚠ Не удалось получить количество ключей: %d", rc);
    }

    if (ble_state.running) ble_advertise_start();
}

static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task запущена");

    // nimble_port_run() вызывает sync_cb, затем крутит цикл событий
    nimble_port_run();
    nimble_port_freertos_deinit();

    ble_state.running = false;
    ble_state.advertising = false;
    ESP_LOGI(TAG, "BLE Host Task завершена");
}

// ==========================================
// Инициализация
// ==========================================

static void ble_do_init(void) {
    int rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "   [1/7] nimble_port_init OK");

    // Помечаем сразу, чтобы settings_tile знал; ble_on_sync проверяет running
    ble_state.initialized = true;
    ble_state.running = true;

    ble_log_stream_init();
    ble_config_tx_power();
    ble_config_security();

    ble_hs_cfg.sync_cb = ble_on_sync;

    rc = ble_svc_gap_device_name_set("DharmaX");
    assert(rc == 0);
    ble_svc_gap_init();

    rc = ble_gatt_init();
    assert(rc == 0);

    // Загружает ключи из NVS синхронно
    ble_store_config_init();
    ble_state.bonds_loaded = true;

#ifdef BLE_DEBUG_BONDS_SELFTEST
    ble_bonds_selftest();
#endif

    // Реклама стартует автоматически в ble_on_sync
    nimble_port_freertos_init(ble_host_task);
    ble_log_sender_start();

    ESP_LOGI(TAG, "🚀 BLE инициализирован (action=0x%04X, logs=0x%04X)",
             ble_state.action_chr_val_handle, ble_state.log_chr_val_handle);

    if (ble_state.log_chr_val_handle == 0) {
        ESP_LOGE(TAG, "❌ log_chr_val_handle = 0, характеристика логов не зарегистрирована!");
    }
}

// ==========================================
// PUBLIC API
// ==========================================

void svc_ble_sync_start(void) {
    if (ble_state.running) {
        ESP_LOGW(TAG, "BLE уже работает");
        return;
    }

    if (!ble_state.initialized) {
        ble_do_init();
    } else {
        // Повторный запуск — просто снова реклама, без reinit
        ble_state.running = true;
        ble_advertise_start();
        ESP_LOGI(TAG, "▶️ BLE возобновлён (быстрый старт)");
    }
}

void svc_ble_sync_stop(void) {
    if (!ble_state.running) return;

    ESP_LOGI(TAG, "◄ Остановка BLE...");

    if (ble_state.advertising) {
        ble_gap_adv_stop();
        ble_state.advertising = false;
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (ble_state.active_conn_handle != 0) {
        ble_gap_terminate(ble_state.active_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        ble_state.active_conn_handle = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    // nimble_port не трогаем: он продолжает работать в фоне
    ble_state.running = false;
    ESP_LOGI(TAG, "BLE остановлен (режим ожидания)");
}

bool svc_ble_sync_is_running(void) { return ble_state.running; }

void svc_ble_sync_advertise_stop(void) {
    if (!ble_state.running) return;
    ble_advertise_stop();
}

void svc_ble_sync_advertise_start(void) {
    if (!ble_state.running || ble_state.advertising) return;
    ble_advertise_start();
}

bool svc_ble_sync_is_advertising(void) { return ble_state.advertising; }