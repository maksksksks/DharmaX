#include "svc_ble_sync.h"
#include "app_events.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "sys/time.h"
#include "svc_vibro.h"

void ble_store_config_init(void);

static const char *TAG = "SVC_BLE";

#define BLE_SVC_UUID_TIME_SYNC 0x1234
#define BLE_CHR_UUID_TIME_SYNC 0x5678

static uint16_t time_sync_conn_handle;
static uint8_t own_addr_type;

// Состояние
static bool ble_running = false;
static bool ble_advertising = false;
static bool ble_initialized = false; // ★ Был ли хоть раз инициализирован?

static void ble_advertise(void);

// ==========================================
// GATT CALLBACK (без изменений)
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

static const struct ble_gatt_svc_def gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_UUID_TIME_SYNC),
        .characteristics = (struct ble_gatt_chr_def[]){{
                                                           .uuid = BLE_UUID16_DECLARE(BLE_CHR_UUID_TIME_SYNC),
                                                           .access_cb = time_chr_access,
                                                           .val_handle = &time_sync_conn_handle,
                                                           .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                                                       },
                                                       {0}},
    },
    {0}};

static void gatt_svc_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "Телефон подключен!");
        ble_advertising = false;
        svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_SHORT, LEVEL_MEDIUM, DURATION_DEFAULT);
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Телефон отключен.");
        svc_vibro_send_event(VIBRATION_EVENT_SINGLE_LONG, LEVEL_MEDIUM, DURATION_DEFAULT);
        if (ble_running)
            ble_advertise();
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
    fields.name = (uint8_t *)"WristWatch";
    fields.name_len = strlen("WristWatch");
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

    // 2. Безопасность
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;

    // 3. Callbacks
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svc_register_cb;

    // 4. Имя устройства
    rc = ble_svc_gap_device_name_set("WristWatch");
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

    ESP_LOGI(TAG, "🚀 BLE инициализирован (реклама начнётся после sync)");
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