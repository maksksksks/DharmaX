#include "ble_power.h"
#include "ble_state.h"
#include "ble_advertise.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "host/ble_hs.h"

static const char *TAG = "BLE_PWR";

// Флаг читается/пишется из задач NimBLE host и esp_timer; ошибка в нём приводит максимум
// к лишнему запросу на обновление параметров, поэтому достаточно volatile.
static volatile bool s_conn_fast = false;
static esp_timer_handle_t s_relax_timer = NULL;
static esp_timer_handle_t s_adv_timer = NULL;

static void apply_profile(uint16_t conn_handle, bool fast)
{
    if (conn_handle == 0 || s_conn_fast == fast) return;

    struct ble_gap_upd_params p = {
        .itvl_min = fast ? BLE_FAST_ITVL_MIN : BLE_IDLE_ITVL_MIN,
        .itvl_max = fast ? BLE_FAST_ITVL_MAX : BLE_IDLE_ITVL_MAX,
        .latency = fast ? BLE_FAST_LATENCY : BLE_IDLE_LATENCY,
        .supervision_timeout = fast ? BLE_FAST_TIMEOUT : BLE_IDLE_TIMEOUT,
        .min_ce_len = 0,
        .max_ce_len = 0,
    };
    int rc = ble_gap_update_params(conn_handle, &p);
    if (rc == 0) {
        s_conn_fast = fast;
        ESP_LOGD(TAG, "profile -> %s", fast ? "FAST" : "IDLE");
    } else {
        // 0x... BLE_HS_EALREADY и т.п. - запрос уже в процессе; повторим в следующий раз
        ESP_LOGW(TAG, "update_params(%s) rc=%d", fast ? "FAST" : "IDLE", rc);
    }
}

static void relax_cb(void *arg)
{
    if (ble_state.active_conn_handle == 0) return;

    if (ble_state.log_subscribed) {              // идёт поток логов - держим FAST
        esp_timer_start_once(s_relax_timer, BLE_FAST_HOLD_MS * 1000ULL);
        return;
    }
    apply_profile(ble_state.active_conn_handle, false);
}

void ble_power_activity(void)
{
    const uint16_t h = ble_state.active_conn_handle;
    if (h == 0) return;

    if (s_relax_timer == NULL) {
        const esp_timer_create_args_t a = { .callback = relax_cb, .name = "ble_relax" };
        if (esp_timer_create(&a, &s_relax_timer) != ESP_OK) return;
    }
    apply_profile(h, true);
    esp_timer_stop(s_relax_timer);               // перезапуск окна удержания
    esp_timer_start_once(s_relax_timer, BLE_FAST_HOLD_MS * 1000ULL);
}

void ble_power_on_connect(uint16_t conn_handle)
{
    (void)conn_handle;
    s_conn_fast = false;                         // реальное состояние неизвестно: параметры задал телефон
    ble_power_adv_window_cancel();
    // Сопряжение/первый обмен идут быстро, затем relax_cb переведёт соединение в IDLE.
    ble_power_activity();
}

void ble_power_on_disconnect(void)
{
    s_conn_fast = false;
    if (s_relax_timer) esp_timer_stop(s_relax_timer);
}

// Телефон/хост может изменить параметры сам или отклонить наш запрос - синхронизируем флаг с фактом.
void ble_power_on_conn_update(uint16_t conn_handle)
{
    struct ble_gap_conn_desc d;
    if (ble_gap_conn_find(conn_handle, &d) != 0) return;
    s_conn_fast = (d.conn_itvl <= BLE_FAST_ITVL_MAX);
    ESP_LOGI(TAG, "conn params: itvl=%.1f мс latency=%d timeout=%d мс",
             d.conn_itvl * 1.25f, d.conn_latency, d.supervision_timeout * 10);
}

// ---------------- реклама ----------------

static void adv_slow_cb(void *arg)
{
    if (!ble_state.running || !ble_state.advertising || ble_state.active_conn_handle != 0) return;
    ble_advertise_restart_slow();
}

void ble_power_adv_window_arm(void)
{
    if (s_adv_timer == NULL) {
        const esp_timer_create_args_t a = { .callback = adv_slow_cb, .name = "ble_adv_slow" };
        if (esp_timer_create(&a, &s_adv_timer) != ESP_OK) return;
    }
    esp_timer_stop(s_adv_timer);
    esp_timer_start_once(s_adv_timer, BLE_ADV_FAST_WINDOW_MS * 1000ULL);
}

void ble_power_adv_window_cancel(void)
{
    if (s_adv_timer) esp_timer_stop(s_adv_timer);
}