#include "ble_notify_parser.h"

#include <string.h>

#include "app_events.h"
#include "cJSON.h"
#include "esp_event.h"
#include "esp_log.h"
#include "svc_vibro.h"

static const char *TAG = "BLE_PARSE";

#define POST_TIMEOUT pdMS_TO_TICKS(100)

// ==========================================
// Хелперы
// ==========================================

static void copy_str(cJSON *obj, const char *key, char *dst, size_t dst_size) {
    cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(it) && it->valuestring != NULL) {
        strncpy(dst, it->valuestring, dst_size - 1);
        dst[dst_size - 1] = '\0';
    }
}

#define COPY_STR(obj, key, field) copy_str(obj, key, field, sizeof(field))

static bool get_int(cJSON *obj, const char *key, int *out) {
    cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(it)) {
        *out = it->valueint;
        return true;
    }
    return false;
}

static void fill_notify_data(cJSON *data, notify_data *out) {
    memset(out, 0, sizeof(*out));
    COPY_STR(data, "id", out->id);
    COPY_STR(data, "app", out->app);
    COPY_STR(data, "title", out->title);
    COPY_STR(data, "txt", out->txt);
    COPY_STR(data, "grp", out->grp);
    int ts;
    if (get_int(data, "ts", &ts)) out->ts = ts;
}

// Возвращает true, если id присутствует (без него удалять нечего)
static bool fill_del_data(cJSON *data, notify_del_data *out) {
    memset(out, 0, sizeof(*out));
    cJSON *id = cJSON_GetObjectItemCaseSensitive(data, "id");
    if (!cJSON_IsString(id) || id->valuestring == NULL) return false;
    COPY_STR(data, "id", out->id);
    COPY_STR(data, "grp", out->grp);
    return true;
}

// ==========================================
// Обработчики пакетов
// ==========================================

static void handle_n_new(cJSON *data) {
    svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_LONG, LEVEL_HIGH, DURATION_LONG);
    notify_data n;
    fill_notify_data(data, &n);
    ESP_LOGI(TAG, "Новое уведомление: [%s] %s - %s", n.app, n.title, n.txt);
    esp_event_post(APP_EVENTS, EVT_BLE_NOTIFY_RX, &n, sizeof(n), POST_TIMEOUT);
}

static void handle_n_del(cJSON *data) {
    svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_LONG, LEVEL_MEDIUM, DURATION_LONG);
    notify_del_data d;
    if (!fill_del_data(data, &d)) return;
    ESP_LOGI(TAG, "Удаление уведомления: %s", d.id);
    esp_event_post(APP_EVENTS, EVT_BLE_NOTIFY_DEL, &d, sizeof(d), POST_TIMEOUT);
}

static void handle_e_new(cJSON *data) {
    svc_vibro_send_event(VIBRATION_EVENT_SINGLE_SHORT, LEVEL_MEDIUM, DURATION_DEFAULT);
    notify_data e;
    fill_notify_data(data, &e);
    ESP_LOGI(TAG, "Новое событие: [%s] %s - %s", e.app, e.title, e.txt);
    esp_event_post(APP_EVENTS, EVT_BLE_EVENT_RX, &e, sizeof(e), POST_TIMEOUT);
}

static void handle_e_del(cJSON *data) {
    notify_del_data d;
    if (!fill_del_data(data, &d)) return;
    esp_event_post(APP_EVENTS, EVT_BLE_EVENT_DEL, &d, sizeof(d), POST_TIMEOUT);
}

static void handle_news_new(cJSON *data) {
    notify_data n;
    fill_notify_data(data, &n);
    ESP_LOGI(TAG, "Новость: [%s] %s", n.app, n.title);
    esp_event_post(APP_EVENTS, EVT_BLE_NEWS_RX, &n, sizeof(n), POST_TIMEOUT);
}

static void handle_media(cJSON *data) {
    media_data_t m;
    memset(&m, 0, sizeof(m));
    COPY_STR(data, "title", m.title);
    COPY_STR(data, "artist", m.artist);

    int v;
    if (get_int(data, "pos", &v)) m.position_sec = (uint32_t)v;
    if (get_int(data, "dur", &v)) m.duration_sec = (uint32_t)v;

    cJSON *playing = cJSON_GetObjectItemCaseSensitive(data, "playing");
    if (cJSON_IsBool(playing)) m.is_playing = cJSON_IsTrue(playing);

    esp_event_post(APP_EVENTS, EVT_BLE_MEDIA_UPDATED, &m, sizeof(m), POST_TIMEOUT);
}

static void handle_phone_status(cJSON *data) {
    phone_status_data_t p;
    memset(&p, 0, sizeof(p));
    p.signal_percent = -1;
    p.battery_percent = -1;

    int v;
    if (get_int(data, "signal", &v)) p.signal_percent = (int8_t)v;
    if (get_int(data, "battery", &v)) p.battery_percent = (int8_t)v;

    cJSON *charging = cJSON_GetObjectItemCaseSensitive(data, "charging");
    if (cJSON_IsBool(charging)) p.charging = cJSON_IsTrue(charging);

    COPY_STR(data, "carrier", p.carrier);

    esp_event_post(APP_EVENTS, EVT_BLE_PHONE_STATUS_UPDATED, &p, sizeof(p), POST_TIMEOUT);
}

// ==========================================
// Таблица диспетчеризации
// ==========================================

typedef struct {
    const char *type;
    void (*handler)(cJSON *data);
} packet_handler_t;

static const packet_handler_t handlers[] = {
    {"n_new", handle_n_new},
    {"n_del", handle_n_del},
    {"e_new", handle_e_new},
    {"e_del", handle_e_del},
    {"news_new", handle_news_new},
    {"media", handle_media},
    {"phone_status", handle_phone_status},
};

void ble_notify_parse_json(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGE(TAG, "Ошибка парсинга JSON");
        return;
    }

    cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "t");
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "d");

    if (cJSON_IsString(type) && type->valuestring != NULL && cJSON_IsObject(data)) {
        bool found = false;
        for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
            if (strcmp(type->valuestring, handlers[i].type) == 0) {
                handlers[i].handler(data);
                found = true;
                break;
            }
        }
        if (!found) {
            ESP_LOGW(TAG, "Неизвестный тип пакета: %s", type->valuestring);
        }
    }

    cJSON_Delete(root);
}