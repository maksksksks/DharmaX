#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "app_events.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file svc_store.h
 * @brief Пассивное хранилище данных приложения.
 *
 * Принципы:
 *  - Только обычная (внутренняя) RAM, статические массивы: ни malloc, ни PSRAM.
 *  - Нет своей задачи и нет таймеров: код выполняется в контексте вызывающего
 *    (BLE-хост, сервисы датчиков, LVGL), энергию хранилище само не тратит.
 *  - Запись и чтение - под мьютексом; читатель получает КОПИЮ.
 *  - После записи (мьютекс уже отпущен) публикуется событие EVT_STORE_* в
 *    APP_EVENTS. Снимки (батарея, RSSI, погода, медиа...) публикуются только
 *    если значение реально изменилось - лишних пробуждений UI нет.
 *
 * Контракт для экранов: create() = прочитать store_get_*() и подписаться на
 * EVT_STORE_*; destroy() = отписаться. Данные от телефона, пришедшие, когда
 * экрана нет, не теряются.
 */

// ---- ёмкости колец (можно переопределить -DSTORE_NOTIFY_CAP=... в CMake) ----
#ifndef STORE_NOTIFY_CAP
#define STORE_NOTIFY_CAP   50
#endif
#ifndef STORE_EVENT_CAP
#define STORE_EVENT_CAP    30
#endif
#ifndef STORE_NEWS_CAP
#define STORE_NEWS_CAP     30
#endif

#define STORE_STEP_BUCKETS 24            // шаги по часам текущих суток
#define STORE_RSSI_NONE    INT8_MIN      // телефон не подключён / RSSI неизвестен

// Гистерезис публикации (меньше изменений - меньше событий - меньше работы UI)
#define STORE_BATTERY_VOLT_STEP  0.05f   // В
#define STORE_RSSI_STEP          6       // дБ

// Последний снимок погоды (источник - пакет телефона; значения в системе СИ)
typedef struct {
    float    temp_c;
    float    feels_like_c;
    float    wind_ms;
    uint8_t  humidity_pct;
    uint8_t  condition;      // код условий (трактует экран погоды)
    uint32_t ts;             // Unix-время снимка, 0 = снимка нет
} weather_snapshot_t;

/** Создаёт мьютекс. Идемпотентна. Вызвать один раз в app_main ДО запуска BLE и UI. */
bool svc_store_init(void);

// ---------------------------------------------------------------------------
// ЗАПИСЬ (BLE-обработчики, сервисы датчиков)
// ---------------------------------------------------------------------------

/** Добавляет запись в кольцо (самая старая вытесняется). Если запись с таким же
 *  непустым id уже есть, она заменяется и становится самой новой. */
void store_put_notification(const notify_data *n);
void store_put_event(const notify_data *e);
void store_put_news(const notify_data *news);

/** Удаляет по группе (если grp непустой - ВСЕ записи группы), иначе по id.
 *  Событие REMOVED публикуется только если что-то действительно удалено. */
void store_del_notification(const char *id, const char *grp);
void store_del_event(const char *id, const char *grp);

/** total - счётчик шагов от датчика. Прирост относится к корзине текущего часа;
 *  при смене суток корзины обнуляются. Падение total (сброс датчика) корзины не трогает. */
void store_set_steps(uint32_t total, float intensity);
void store_set_battery(int percent, float voltage);
void store_set_weather(const weather_snapshot_t *w);
void store_set_rssi(int8_t rssi_dbm);                   // STORE_RSSI_NONE = нет связи
void store_set_media(const media_data_t *m);
void store_set_phone_status(const phone_status_data_t *p);

// ---------------------------------------------------------------------------
// ЧТЕНИЕ (экран при create()). Всё - копии под мьютексом.
// ---------------------------------------------------------------------------

/** Копирует до max_count записей в out в ХРОНОЛОГИЧЕСКОМ порядке (старые первыми,
 *  чтобы последовательные card_feed_add дали ту же ленту, что и вживую).
 *  Если записей больше max_count - возвращаются самые новые. Возврат - число записей. */
size_t store_get_notifications(notify_data *out, size_t max_count);
size_t store_get_events(notify_data *out, size_t max_count);
size_t store_get_news(notify_data *out, size_t max_count);

size_t store_count_notifications(void);
size_t store_count_events(void);
size_t store_count_news(void);

uint32_t store_get_step_count(void);
bool     store_get_steps(motion_step_data_t *out);                 // false - шагов ещё не было
void     store_get_step_buckets(uint16_t out[STORE_STEP_BUCKETS]);

bool store_get_battery(battery_data_t *out);                      // false - ещё не измеряли
bool store_get_weather(weather_snapshot_t *out);                  // false - снимка нет
bool store_get_rssi(int8_t *out);                                 // false - нет связи
bool store_get_media(media_data_t *out);
bool store_get_phone_status(phone_status_data_t *out);

/** Источник времени (по умолчанию time(NULL)). Нужен тестам и RTC-драйверу. */
void svc_store_set_time_source(time_t (*now)(void));

#ifdef __cplusplus
}
#endif
