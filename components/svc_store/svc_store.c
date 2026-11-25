#include "svc_store.h"

#include <math.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "SVC_STORE";

#define LOCK_TIMEOUT_MS   500
#define POST_TIMEOUT_MS   20      // не блокируем BLE-хост надолго: данные уже в хранилище
#define TIME_VALID_YEAR   124     // tm_year: 2024+, до этого RTC считается несинхронизированным

// ---------------------------------------------------------------------------
// Кольцо записей notify_data. Логический индекс 0 - самая старая запись.
// ---------------------------------------------------------------------------
typedef struct {
    notify_data *items;
    uint16_t cap;
    uint16_t head;     // физический индекс самой старой записи
    uint16_t count;
} ring_t;

static notify_data s_notif_buf[STORE_NOTIFY_CAP];
static notify_data s_event_buf[STORE_EVENT_CAP];
static notify_data s_news_buf[STORE_NEWS_CAP];

static ring_t s_notif = { s_notif_buf, STORE_NOTIFY_CAP, 0, 0 };
static ring_t s_event = { s_event_buf, STORE_EVENT_CAP,  0, 0 };
static ring_t s_news  = { s_news_buf,  STORE_NEWS_CAP,   0, 0 };

static notify_data *ring_at(ring_t *r, size_t i)
{
    return &r->items[(r->head + i) % r->cap];
}

static void ring_push(ring_t *r, const notify_data *n)
{
    if (r->count == r->cap) {                    // полное: затираем самую старую
        r->items[r->head] = *n;
        r->head = (r->head + 1) % r->cap;
    } else {
        *ring_at(r, r->count) = *n;
        r->count++;
    }
}

// Удаляет все записи, подходящие под (id | grp). Один проход со сжатием.
// grp непустой -> по группе; иначе по id. Возвращает число удалённых.
static size_t ring_remove(ring_t *r, const char *id, const char *grp)
{
    const bool by_grp = (grp != NULL && grp[0] != '\0');
    if (!by_grp && (id == NULL || id[0] == '\0')) return 0;

    size_t w = 0;
    for (size_t i = 0; i < r->count; i++) {
        const notify_data *it = ring_at(r, i);
        const bool match = by_grp ? (strcmp(it->grp, grp) == 0) : (strcmp(it->id, id) == 0);
        if (match) continue;
        if (w != i) *ring_at(r, w) = *it;
        w++;
    }
    size_t removed = r->count - w;
    r->count = (uint16_t)w;
    return removed;
}

static size_t ring_copy_out(ring_t *r, notify_data *out, size_t max_count)
{
    if (out == NULL || max_count == 0) return 0;
    size_t n = r->count < max_count ? r->count : max_count;
    size_t skip = r->count - n;                  // при нехватке места берём самые новые
    for (size_t i = 0; i < n; i++) out[i] = *ring_at(r, skip + i);
    return n;
}

// ---------------------------------------------------------------------------
// Снимки
// ---------------------------------------------------------------------------
static struct {
    uint32_t total;
    float intensity;
    bool valid;
    int day;                                  // tm_yday, -1 = ещё не знаем
    uint16_t buckets[STORE_STEP_BUCKETS];
} s_steps = { .day = -1 };

static battery_data_t s_bat, s_bat_pub;       // s_bat_pub - последнее ОПУБЛИКОВАННОЕ
static bool s_bat_valid, s_bat_pub_valid;

static weather_snapshot_t s_weather;
static bool s_weather_valid;

static int8_t s_rssi = STORE_RSSI_NONE, s_rssi_pub = STORE_RSSI_NONE;

static media_data_t s_media;
static bool s_media_valid;

static phone_status_data_t s_phone;
static bool s_phone_valid;

// ---------------------------------------------------------------------------
// Инфраструктура
// ---------------------------------------------------------------------------
static StaticSemaphore_t s_mtx_storage;
static SemaphoreHandle_t s_mtx = NULL;
static time_t (*s_now)(void) = NULL;

static bool lock(void)
{
    if (s_mtx == NULL) {
        ESP_LOGE(TAG, "svc_store_init() не вызван");
        return false;
    }
    return xSemaphoreTake(s_mtx, pdMS_TO_TICKS(LOCK_TIMEOUT_MS)) == pdTRUE;
}

static void unlock(void)
{
    xSemaphoreGive(s_mtx);
}

static void publish(int32_t evt, const void *data, size_t size)
{
    // Вызывать только ПОСЛЕ unlock(): подписчик может сразу прочитать хранилище.
    // esp_event копирует data, поэтому передавать указатель на локальную копию безопасно.
    esp_err_t err = esp_event_post(APP_EVENTS, evt, (void *)data, size, pdMS_TO_TICKS(POST_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "post evt=%d failed (%d): подписчик увидит данные при следующем чтении", (int)evt, (int)err);
    }
}

static time_t now_time(void)
{
    return s_now ? s_now() : time(NULL);
}

bool svc_store_init(void)
{
    if (s_mtx == NULL) {
        s_mtx = xSemaphoreCreateMutexStatic(&s_mtx_storage);   // без кучи
    }
    return s_mtx != NULL;
}

void svc_store_set_time_source(time_t (*now)(void))
{
    s_now = now;
}

// ---------------------------------------------------------------------------
// Кольца: запись
// ---------------------------------------------------------------------------
static void terminate(notify_data *n)
{
    n->id[sizeof(n->id) - 1] = '\0';
    n->grp[sizeof(n->grp) - 1] = '\0';
    n->app[sizeof(n->app) - 1] = '\0';
    n->title[sizeof(n->title) - 1] = '\0';
    n->txt[sizeof(n->txt) - 1] = '\0';
}

static void put(ring_t *r, const notify_data *src, int32_t evt)
{
    if (src == NULL) return;
    notify_data n = *src;                          // 374 байта на стеке вызывающего потока
    terminate(&n);

    if (!lock()) return;
    if (n.id[0] != '\0') ring_remove(r, n.id, NULL);   // обновление существующего: новая копия станет самой свежей
    ring_push(r, &n);
    unlock();

    publish(evt, &n, sizeof(n));
}

static void del(ring_t *r, const char *id, const char *grp, int32_t evt)
{
    notify_del_data d;
    memset(&d, 0, sizeof(d));
    if (id)  strncpy(d.id, id, sizeof(d.id) - 1);
    if (grp) strncpy(d.grp, grp, sizeof(d.grp) - 1);

    if (!lock()) return;
    size_t removed = ring_remove(r, d.id, d.grp);
    unlock();

    if (removed > 0) publish(evt, &d, sizeof(d));
}

void store_put_notification(const notify_data *n) { put(&s_notif, n, EVT_STORE_NOTIFICATION_ADDED); }
void store_put_event(const notify_data *e)        { put(&s_event, e, EVT_STORE_EVENT_ADDED); }
void store_put_news(const notify_data *news)      { put(&s_news,  news, EVT_STORE_NEWS_ADDED); }

void store_del_notification(const char *id, const char *grp) { del(&s_notif, id, grp, EVT_STORE_NOTIFICATION_REMOVED); }
void store_del_event(const char *id, const char *grp)        { del(&s_event, id, grp, EVT_STORE_EVENT_REMOVED); }

// ---------------------------------------------------------------------------
// Снимки: запись
// ---------------------------------------------------------------------------
void store_set_steps(uint32_t total, float intensity)
{
    struct tm tm_now;
    time_t t = now_time();
    localtime_r(&t, &tm_now);
    const bool synced = tm_now.tm_year >= TIME_VALID_YEAR;

    motion_step_data_t evt;
    bool changed;

    if (!lock()) return;

    if (synced && tm_now.tm_yday != s_steps.day) {
        memset(s_steps.buckets, 0, sizeof(s_steps.buckets));   // новые сутки
        s_steps.day = tm_now.tm_yday;
    }
    if (s_steps.valid && total > s_steps.total) {
        uint32_t delta = total - s_steps.total;
        uint32_t idx = synced ? (uint32_t)tm_now.tm_hour : 0;
        uint32_t sum = (uint32_t)s_steps.buckets[idx] + delta;
        s_steps.buckets[idx] = sum > UINT16_MAX ? UINT16_MAX : (uint16_t)sum;
    }
    changed = !s_steps.valid || total != s_steps.total;
    s_steps.total = total;
    s_steps.intensity = intensity;
    s_steps.valid = true;
    evt.steps = total;
    evt.intensity = intensity;

    unlock();
    if (changed) publish(EVT_STORE_STEPS_UPDATED, &evt, sizeof(evt));
}

void store_set_battery(int percent, float voltage)
{
    battery_data_t evt;
    bool publish_it;

    if (!lock()) return;
    s_bat.percent = percent;
    s_bat.voltage = voltage;
    s_bat_valid = true;
    // Сравниваем с последним ОПУБЛИКОВАННЫМ, иначе медленный дрейф не накопится до порога
    publish_it = !s_bat_pub_valid ||
                 (percent != s_bat_pub.percent) ||
                 (fabsf(voltage - s_bat_pub.voltage) >= STORE_BATTERY_VOLT_STEP);
    if (publish_it) {
        s_bat_pub = s_bat;
        s_bat_pub_valid = true;
    }
    evt = s_bat;
    unlock();

    if (publish_it) publish(EVT_STORE_BATTERY_UPDATED, &evt, sizeof(evt));
}

void store_set_weather(const weather_snapshot_t *w)
{
    if (w == NULL) return;
    weather_snapshot_t evt = *w;
    bool changed;

    if (!lock()) return;
    changed = !s_weather_valid || memcmp(&s_weather, &evt, sizeof(evt)) != 0;
    s_weather = evt;
    s_weather_valid = true;
    unlock();

    if (changed) publish(EVT_STORE_WEATHER_UPDATED, &evt, sizeof(evt));
}

void store_set_rssi(int8_t rssi_dbm)
{
    bool publish_it;

    if (!lock()) return;
    s_rssi = rssi_dbm;
    const bool was_none = (s_rssi_pub == STORE_RSSI_NONE);
    const bool is_none = (rssi_dbm == STORE_RSSI_NONE);
    if (was_none != is_none) {
        publish_it = true;                                       // связь появилась/пропала
    } else if (is_none) {
        publish_it = false;
    } else {
        int diff = rssi_dbm - s_rssi_pub;
        publish_it = (diff < 0 ? -diff : diff) >= STORE_RSSI_STEP;
    }
    if (publish_it) s_rssi_pub = rssi_dbm;
    unlock();

    if (publish_it) publish(EVT_STORE_RSSI_UPDATED, &rssi_dbm, sizeof(rssi_dbm));
}

void store_set_media(const media_data_t *m)
{
    if (m == NULL) return;
    media_data_t evt = *m;
    bool changed;

    if (!lock()) return;
    changed = !s_media_valid || memcmp(&s_media, &evt, sizeof(evt)) != 0;
    s_media = evt;
    s_media_valid = true;
    unlock();

    if (changed) publish(EVT_STORE_MEDIA_UPDATED, &evt, sizeof(evt));
}

void store_set_phone_status(const phone_status_data_t *p)
{
    if (p == NULL) return;
    phone_status_data_t evt = *p;
    bool changed;

    if (!lock()) return;
    changed = !s_phone_valid || memcmp(&s_phone, &evt, sizeof(evt)) != 0;
    s_phone = evt;
    s_phone_valid = true;
    unlock();

    if (changed) publish(EVT_STORE_PHONE_STATUS_UPDATED, &evt, sizeof(evt));
}

// ---------------------------------------------------------------------------
// Чтение
// ---------------------------------------------------------------------------
static size_t get_ring(ring_t *r, notify_data *out, size_t max_count)
{
    if (!lock()) return 0;
    size_t n = ring_copy_out(r, out, max_count);
    unlock();
    return n;
}

static size_t count_ring(ring_t *r)
{
    if (!lock()) return 0;
    size_t n = r->count;
    unlock();
    return n;
}

size_t store_get_notifications(notify_data *out, size_t max_count) { return get_ring(&s_notif, out, max_count); }
size_t store_get_events(notify_data *out, size_t max_count)        { return get_ring(&s_event, out, max_count); }
size_t store_get_news(notify_data *out, size_t max_count)          { return get_ring(&s_news, out, max_count); }

size_t store_count_notifications(void) { return count_ring(&s_notif); }
size_t store_count_events(void)        { return count_ring(&s_event); }
size_t store_count_news(void)          { return count_ring(&s_news); }

uint32_t store_get_step_count(void)
{
    if (!lock()) return 0;
    uint32_t v = s_steps.total;
    unlock();
    return v;
}

bool store_get_steps(motion_step_data_t *out)
{
    if (out == NULL || !lock()) return false;
    bool ok = s_steps.valid;
    if (ok) {
        out->steps = s_steps.total;
        out->intensity = s_steps.intensity;
    }
    unlock();
    return ok;
}

void store_get_step_buckets(uint16_t out[STORE_STEP_BUCKETS])
{
    if (out == NULL) return;
    if (!lock()) { memset(out, 0, STORE_STEP_BUCKETS * sizeof(uint16_t)); return; }
    memcpy(out, s_steps.buckets, sizeof(s_steps.buckets));
    unlock();
}

bool store_get_battery(battery_data_t *out)
{
    if (out == NULL || !lock()) return false;
    bool ok = s_bat_valid;
    if (ok) *out = s_bat;
    unlock();
    return ok;
}

bool store_get_weather(weather_snapshot_t *out)
{
    if (out == NULL || !lock()) return false;
    bool ok = s_weather_valid;
    if (ok) *out = s_weather;
    unlock();
    return ok;
}

bool store_get_rssi(int8_t *out)
{
    if (out == NULL || !lock()) return false;
    bool ok = (s_rssi != STORE_RSSI_NONE);
    if (ok) *out = s_rssi;
    unlock();
    return ok;
}

bool store_get_media(media_data_t *out)
{
    if (out == NULL || !lock()) return false;
    bool ok = s_media_valid;
    if (ok) *out = s_media;
    unlock();
    return ok;
}

bool store_get_phone_status(phone_status_data_t *out)
{
    if (out == NULL || !lock()) return false;
    bool ok = s_phone_valid;
    if (ok) *out = s_phone;
    unlock();
    return ok;
}
