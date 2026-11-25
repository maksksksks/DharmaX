#include "screen_notify_logic.h"
#include "ui.h"
#include "card_feed.h"
#include "app_events.h"
#include "esp_event.h"
#include "esp_log.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "SCREEN_NOTIFY";
static card_feed_t s_feed = NULL;   // переживает экран (см. screen_notify_service_init)
static app_evt_sub_t s_sub_rx, s_sub_del;

static void apply_theme(void)
{
    lv_obj_set_style_text_color(ui_Label8, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_y(ui_Label8, -210); // поднимаем заголовок, освобождая место под ленту
}

static void build_feed(void)
{
    // Лента живёт дольше экрана, здесь её только подключаем к свежему экрану.
    card_feed_attach(s_feed, ui_ScreenNotify);
    lv_obj_t *container = card_feed_get_container(s_feed);
    lv_obj_set_size(container, lv_pct(100), lv_pct(78));
    lv_obj_set_align(container, LV_ALIGN_BOTTOM_MID);
}

static void notify_to_card(const notify_data *n, card_feed_item_t *out, char *time_buf, size_t time_buf_sz)
{
    time_t raw = (time_t)n->ts;
    struct tm tm_info;
    localtime_r(&raw, &tm_info);
    strftime(time_buf, time_buf_sz, "%H:%M", &tm_info);

    out->id = n->id;
    out->grp = n->grp;
    out->header_left = n->app;
    out->header_right = time_buf;
    out->title = (n->title[0] && strcmp(n->app, n->title) != 0) ? n->title : "";
    out->body = n->txt;
}

static void ble_notify_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (s_feed == NULL) return;
    (void)arg; (void)base;
    if (id == EVT_BLE_NOTIFY_RX) {
        notify_data *n = (notify_data *)event_data;
        card_feed_item_t item;
        char time_buf[6];
        notify_to_card(n, &item, time_buf, sizeof(time_buf));
        card_feed_add(s_feed, &item);
    } else if (id == EVT_BLE_NOTIFY_DEL) {
        notify_del_data *d = (notify_del_data *)event_data;
        if (d->grp[0]) card_feed_remove_by_group(s_feed, d->grp);
        else card_feed_remove_by_id(s_feed, d->id);
    }
}

// Сборщик данных: вызывается один раз при старте приложения (app_ui_init) и
// принимает уведомления и когда экрана нет. Это слой данных, а не подписка экрана,
// поэтому destroy() его не трогает. Идемпотентна.
void screen_notify_service_init(void)
{
    if (s_feed != NULL) return;
    s_feed = card_feed_create_detached(600000);
    app_evt_subscribe(&s_sub_rx, EVT_BLE_NOTIFY_RX, ble_notify_event_handler, NULL);
    app_evt_subscribe(&s_sub_del, EVT_BLE_NOTIFY_DEL, ble_notify_event_handler, NULL);
}

static void logic_init(void)
{
    screen_notify_service_init();   // на случай, если экран создали раньше сервиса
    apply_theme();
    build_feed();
    ESP_LOGI(TAG, "Notify screen logic initialized");
}

static void logic_deinit(void)
{
    card_feed_detach(s_feed);   // лента (и её карточки) остаётся жить в скрытом контейнере
}

lv_obj_t *screen_notify_create(void)
{
    ui_ScreenNotify_screen_init();
    logic_init();
    return ui_ScreenNotify;
}

void screen_notify_destroy(lv_obj_t *scr)
{
    (void)scr;                        // экран - синглтон, его указатель лежит в ui_ScreenNotify
    logic_deinit();                   // 1) подписки  2) таймеры  3) указатели на виджеты
    ui_ScreenNotify_screen_destroy();     // lv_obj_del + обнуление глобальных ui_*
}
