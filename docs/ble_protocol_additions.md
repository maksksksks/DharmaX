# Расширение BLE-протокола: события, новости, плеер, статус телефона

Экраны `screen_events`, `screen_news`, `screen_phone`, `screen_sys` рассчитаны на
данные с телефона, которых пока нет в вашем `svc_ble_sync.c`. Он уже отлично
устроен под JSON-пакеты с полем `"t"` (тип) в характеристике NOTIFY
(`BLE_CHR_UUID_NOTIFY`) - просто добавляем новые типы по той же схеме, что
`n_new` / `n_del`.

Ничего в существующей логике (time sync, pairing, notify, log stream) менять
не нужно - только добавить veтки в `notify_chr_access()` и несколько
esp_event ID.

## 1. `app_events.h` - добавить

```c
// ==== СОБЫТИЯ (календарь с телефона) ====
ESP_EVENT_DECLARE_BASE(APP_EVENTS); // уже объявлено у вас, просто новые ID:

// в enum/список ID рядом с EVT_BLE_NOTIFY_RX / EVT_BLE_NOTIFY_DEL:
EVT_BLE_EVENT_RX,     // новое событие календаря -> notify_data (переиспользуем структуру)
EVT_BLE_EVENT_DEL,    // удаление события -> notify_del_data

EVT_BLE_NEWS_RX,      // новость -> notify_data

EVT_BLE_MEDIA_UPDATED,      // обновление состояния плеера -> media_data_t
EVT_BLE_PHONE_STATUS_UPDATED, // статус телефона -> phone_status_data_t

// команды часы -> телефон (через send_action_to_phone), рядом с существующими action-кодами:
#define ACTION_MEDIA_PLAY_PAUSE 0x10
#define ACTION_MEDIA_NEXT       0x11
#define ACTION_MEDIA_PREV       0x12

// новые структуры данных:
typedef struct {
    char title[64];
    char artist[64];
    uint32_t position_sec;
    uint32_t duration_sec;
    bool is_playing;
} media_data_t;

typedef struct {
    int8_t signal_percent;   // -1 если неизвестно
    int8_t battery_percent;  // -1 если неизвестно
    bool charging;
    char carrier[16];        // имя оператора, может быть ""
} phone_status_data_t;
```

`notify_data` / `notify_del_data` у вас уже есть (используются для
уведомлений) - для событий и новостей переиспользуем их 1:1, поля те же
(`id`, `app`, `title`, `txt`, `ts`, `grp`).

## 2. `svc_ble_sync.c` - добавить ветки в `notify_chr_access()`

Внутри `notify_chr_access()`, там же где сейчас `if (strcmp(type_item->valuestring, "n_new") == 0) { ... }`
и `else if (strcmp(type_item->valuestring, "n_del") == 0) { ... }`, добавляем ещё три `else if`:

```c
// ==========================================
// СОБЫТИЕ КАЛЕНДАРЯ (e_new / e_del) - тот же формат, что n_new/n_del
// ==========================================
else if (strcmp(type_item->valuestring, "e_new") == 0) {
    svc_vibro_send_event(VIBRATION_EVENT_SINGLE_SHORT, LEVEL_MEDIUM, DURATION_DEFAULT);
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "d");
    if (cJSON_IsObject(data)) {
        notify_data n_data;
        memset(&n_data, 0, sizeof(n_data));

        cJSON *id = cJSON_GetObjectItemCaseSensitive(data, "id");
        cJSON *app = cJSON_GetObjectItemCaseSensitive(data, "app");    // напр. имя календаря
        cJSON *title = cJSON_GetObjectItemCaseSensitive(data, "title");
        cJSON *txt = cJSON_GetObjectItemCaseSensitive(data, "txt");
        cJSON *ts = cJSON_GetObjectItemCaseSensitive(data, "ts");
        cJSON *grp = cJSON_GetObjectItemCaseSensitive(data, "grp");

        if (cJSON_IsString(id)) strncpy(n_data.id, id->valuestring, sizeof(n_data.id) - 1);
        if (cJSON_IsString(app)) strncpy(n_data.app, app->valuestring, sizeof(n_data.app) - 1);
        if (cJSON_IsString(title)) strncpy(n_data.title, title->valuestring, sizeof(n_data.title) - 1);
        if (cJSON_IsString(txt)) strncpy(n_data.txt, txt->valuestring, sizeof(n_data.txt) - 1);
        if (cJSON_IsNumber(ts)) n_data.ts = ts->valueint;
        if (cJSON_IsString(grp)) strncpy(n_data.grp, grp->valuestring, sizeof(n_data.grp) - 1);

        esp_event_post(APP_EVENTS, EVT_BLE_EVENT_RX, &n_data, sizeof(n_data), pdMS_TO_TICKS(100));
    }
}
else if (strcmp(type_item->valuestring, "e_del") == 0) {
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "d");
    if (cJSON_IsObject(data)) {
        notify_del_data del_data;
        memset(&del_data, 0, sizeof(del_data));
        cJSON *id = cJSON_GetObjectItemCaseSensitive(data, "id");
        cJSON *grp = cJSON_GetObjectItemCaseSensitive(data, "grp");
        if (cJSON_IsString(id)) strncpy(del_data.id, id->valuestring, sizeof(del_data.id) - 1);
        if (cJSON_IsString(grp)) strncpy(del_data.grp, grp->valuestring, sizeof(del_data.grp) - 1);
        esp_event_post(APP_EVENTS, EVT_BLE_EVENT_DEL, &del_data, sizeof(del_data), pdMS_TO_TICKS(100));
    }
}

// ==========================================
// НОВОСТЬ (news_new) - тот же формат, что n_new, без del
// ==========================================
else if (strcmp(type_item->valuestring, "news_new") == 0) {
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "d");
    if (cJSON_IsObject(data)) {
        notify_data n_data;
        memset(&n_data, 0, sizeof(n_data));

        cJSON *id = cJSON_GetObjectItemCaseSensitive(data, "id");
        cJSON *app = cJSON_GetObjectItemCaseSensitive(data, "app");   // источник новости
        cJSON *title = cJSON_GetObjectItemCaseSensitive(data, "title");
        cJSON *txt = cJSON_GetObjectItemCaseSensitive(data, "txt");
        cJSON *ts = cJSON_GetObjectItemCaseSensitive(data, "ts");

        if (cJSON_IsString(id)) strncpy(n_data.id, id->valuestring, sizeof(n_data.id) - 1);
        if (cJSON_IsString(app)) strncpy(n_data.app, app->valuestring, sizeof(n_data.app) - 1);
        if (cJSON_IsString(title)) strncpy(n_data.title, title->valuestring, sizeof(n_data.title) - 1);
        if (cJSON_IsString(txt)) strncpy(n_data.txt, txt->valuestring, sizeof(n_data.txt) - 1);
        if (cJSON_IsNumber(ts)) n_data.ts = ts->valueint;

        esp_event_post(APP_EVENTS, EVT_BLE_NEWS_RX, &n_data, sizeof(n_data), pdMS_TO_TICKS(100));
    }
}

// ==========================================
// ПЛЕЕР (media) - телефон шлёт текущее состояние при каждом изменении
// ==========================================
else if (strcmp(type_item->valuestring, "media") == 0) {
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "d");
    if (cJSON_IsObject(data)) {
        media_data_t m;
        memset(&m, 0, sizeof(m));

        cJSON *title = cJSON_GetObjectItemCaseSensitive(data, "title");
        cJSON *artist = cJSON_GetObjectItemCaseSensitive(data, "artist");
        cJSON *pos = cJSON_GetObjectItemCaseSensitive(data, "pos");
        cJSON *dur = cJSON_GetObjectItemCaseSensitive(data, "dur");
        cJSON *playing = cJSON_GetObjectItemCaseSensitive(data, "playing");

        if (cJSON_IsString(title)) strncpy(m.title, title->valuestring, sizeof(m.title) - 1);
        if (cJSON_IsString(artist)) strncpy(m.artist, artist->valuestring, sizeof(m.artist) - 1);
        if (cJSON_IsNumber(pos)) m.position_sec = (uint32_t)pos->valueint;
        if (cJSON_IsNumber(dur)) m.duration_sec = (uint32_t)dur->valueint;
        if (cJSON_IsBool(playing)) m.is_playing = cJSON_IsTrue(playing);

        esp_event_post(APP_EVENTS, EVT_BLE_MEDIA_UPDATED, &m, sizeof(m), pdMS_TO_TICKS(100));
    }
}

// ==========================================
// СТАТУС ТЕЛЕФОНА (phone_status) - связь/батарея/оператор
// ==========================================
else if (strcmp(type_item->valuestring, "phone_status") == 0) {
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "d");
    if (cJSON_IsObject(data)) {
        phone_status_data_t ps;
        memset(&ps, 0, sizeof(ps));
        ps.signal_percent = -1;
        ps.battery_percent = -1;

        cJSON *signal = cJSON_GetObjectItemCaseSensitive(data, "signal");
        cJSON *battery = cJSON_GetObjectItemCaseSensitive(data, "battery");
        cJSON *charging = cJSON_GetObjectItemCaseSensitive(data, "charging");
        cJSON *carrier = cJSON_GetObjectItemCaseSensitive(data, "carrier");

        if (cJSON_IsNumber(signal)) ps.signal_percent = (int8_t)signal->valueint;
        if (cJSON_IsNumber(battery)) ps.battery_percent = (int8_t)battery->valueint;
        if (cJSON_IsBool(charging)) ps.charging = cJSON_IsTrue(charging);
        if (cJSON_IsString(carrier)) strncpy(ps.carrier, carrier->valuestring, sizeof(ps.carrier) - 1);

        esp_event_post(APP_EVENTS, EVT_BLE_PHONE_STATUS_UPDATED, &ps, sizeof(ps), pdMS_TO_TICKS(100));
    }
}
```

Вставлять эти пять `else if` нужно **до** финальной закрывающей скобки цепочки
`if/else if` (то есть сразу после существующего блока `n_del`, перед `}` что
закрывает `if (cJSON_IsString(type_item) ...)`).

## 3. Команды с часов на телефон (плеер)

`send_action_to_phone()` у вас уже реализован и используется в settings
(`svc_auto_brightness`). Экран плеера (`screen_phone`) переиспользует его же:

```c
send_action_to_phone(ACTION_MEDIA_PLAY_PAUSE);
send_action_to_phone(ACTION_MEDIA_NEXT);
send_action_to_phone(ACTION_MEDIA_PREV);
```

На стороне приложения на телефоне нужно только обработать эти три кода в
характеристике `BLE_CHR_UUID_ACTION` (0x5680) так же, как уже обрабатываются
0x01/0x02 для auto-brightness.

## 4. Итого какие константы должны совпадать со стороной телефона

| Тип пакета (`"t"`) | Направление | Обработчик добавлен в |
|---|---|---|
| `e_new` / `e_del` | телефон → часы | `notify_chr_access` (выше) |
| `news_new` | телефон → часы | `notify_chr_access` (выше) |
| `media` | телефон → часы | `notify_chr_access` (выше) |
| `phone_status` | телефон → часы | `notify_chr_access` (выше) |
| `0x10/0x11/0x12` (action) | часы → телефон | уже существующий `action_chr` |

Форматы полей (`id`, `app`, `title`, `txt`, `ts`, `grp` для событий/новостей;
`title`, `artist`, `pos`, `dur`, `playing` для плеера; `signal`, `battery`,
`charging`, `carrier` для статуса) нужно зафиксировать и на стороне
Android/iOS приложения - это единственная часть работы, которую нельзя
сделать в прошивке.
