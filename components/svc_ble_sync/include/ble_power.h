#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Энергосберегающее управление BLE-соединением и рекламой.
 *
 *  Соединение:
 *    IDLE  - интервал 400-500 мс, latency 0, timeout 6 с. Радио просыпается ~2 раза/с.
 *            Задержка доставки команды с телефона <= интервала (~0.5 с).
 *    FAST  - интервал 30-60 мс на время обмена (запись от телефона, отправка на телефон,
 *            сопряжение, поток логов). Через BLE_FAST_HOLD_MS без активности -> IDLE.
 *  Реклама:
 *    первые BLE_ADV_FAST_WINDOW_MS - 60-100 мс (быстрое переподключение),
 *    затем 1.0-1.2 с (телефон вне зоны может быть часами).
 *
 * iOS: itvl_min >= 15 мс, itvl_max*(latency+1) <= 2 с, timeout <= 6 с - соблюдено.
 */

// --- профили соединения (единицы: интервал 1.25 мс, timeout 10 мс) ---
#define BLE_IDLE_ITVL_MIN    0x0280   // 400 мс
#define BLE_IDLE_ITVL_MAX    0x0320   // 500 мс
#define BLE_IDLE_LATENCY     0
#define BLE_IDLE_TIMEOUT     600      // 6 с

#define BLE_FAST_ITVL_MIN    0x0018   // 24*1.25 = 30 мс
#define BLE_FAST_ITVL_MAX    0x0030   // 48*1.25 = 60 мс
#define BLE_FAST_LATENCY     0
#define BLE_FAST_TIMEOUT     400      // 4 с

#define BLE_FAST_HOLD_MS     4000

// --- реклама (единицы 0.625 мс) ---
#define BLE_ADV_FAST_MIN        0x0060   // 60 мс
#define BLE_ADV_FAST_MAX        0x0080   // 80 мс
#define BLE_ADV_SLOW_MIN        0x0640   // 1000 мс
#define BLE_ADV_SLOW_MAX        0x0780   // 1200 мс
#define BLE_ADV_FAST_WINDOW_MS  30000

// Любая реальная активность обмена. Безопасно вызывать из любой задачи.
void ble_power_activity(void);

// События жизненного цикла (вызываются из ble_gap_handler / ble_advertise)
void ble_power_on_connect(uint16_t conn_handle);
void ble_power_on_disconnect(void);
void ble_power_on_conn_update(uint16_t conn_handle);   // BLE_GAP_EVENT_CONN_UPDATE

// Реклама: окно быстрой рекламы
void ble_power_adv_window_arm(void);
void ble_power_adv_window_cancel(void);