#include "screen_news_logic.h"
#include "ui.h"
#include "card_feed.h"
#include "app_events.h"
#include "esp_event.h"
#include "esp_log.h"
#include <time.h>
#include <string.h>

// ВНИМАНИЕ: EVT_BLE_NEWS_RX - новый тип события, реализуется по образцу
// EVT_BLE_NOTIFY_RX в svc_ble_sync.c (тип пакета "news_new" вместо "n_new").
// См. components/docs/ble_protocol_additions.md

static const char *TAG = "SCREEN_NEWS";
static card_feed_t s_feed;

static void apply_theme(void)
{
    lv_obj_set_style_text_color(ui_Label16, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_y(ui_Label16, -210);
}

static void build_feed(void)
{
    s_feed = card_feed_create(ui_ScreenNews, 0); // новости живут, пока их не смахнут
    lv_obj_t *container = card_feed_get_container(s_feed);
    lv_obj_set_size(container, lv_pct(100), lv_pct(78));
    lv_obj_set_align(container, LV_ALIGN_BOTTOM_MID);
}

static void news_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg; (void)base; (void)id;
    notify_data *n = (notify_data *)event_data;

    time_t raw = (time_t)n->ts;
    struct tm tm_info;
    localtime_r(&raw, &tm_info);
    char time_buf[6];
    strftime(time_buf, sizeof(time_buf), "%H:%M", &tm_info);

    card_feed_item_t item = {
        .id = n->id,
        .grp = n->grp,
        .header_left = n->app,   // напр. источник новости
        .header_right = time_buf,
        .title = n->title,
        .body = n->txt,
    };
    card_feed_add(s_feed, &item);
}

void screen_news_logic_init(void)
{
    apply_theme();
    build_feed();

    esp_event_handler_instance_register(APP_EVENTS, EVT_BLE_NEWS_RX, news_event_handler, NULL, NULL);

    ESP_LOGI(TAG, "News screen logic initialized");
}

void ui_ScreenNews_full_init(void)
{
    ui_ScreenNews_screen_init();
    screen_news_logic_init();
}