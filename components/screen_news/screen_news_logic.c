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
static card_feed_t s_feed = NULL;   // переживает экран (см. screen_news_service_init)
static app_evt_sub_t s_sub_rx;

static void apply_theme(void)
{
    lv_obj_set_style_text_color(ui_Label16, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_y(ui_Label16, -210);
}

static void build_feed(void)
{
    // Лента живёт дольше экрана, здесь её только подключаем к свежему экрану.
    card_feed_attach(s_feed, ui_ScreenNews);
    lv_obj_t *container = card_feed_get_container(s_feed);
    lv_obj_set_size(container, lv_pct(100), lv_pct(78));
    lv_obj_set_align(container, LV_ALIGN_BOTTOM_MID);
}

static void news_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (s_feed == NULL) return;
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

// Сборщик данных: вызывается один раз при старте приложения (app_ui_init) и
// принимает новости и когда экрана нет. Это слой данных, а не подписка экрана,
// поэтому destroy() его не трогает. Идемпотентна.
void screen_news_service_init(void)
{
    if (s_feed != NULL) return;
    s_feed = card_feed_create_detached(0);
    app_evt_subscribe(&s_sub_rx, EVT_BLE_NEWS_RX, news_event_handler, NULL);
}

static void logic_init(void)
{
    screen_news_service_init();   // на случай, если экран создали раньше сервиса
    apply_theme();
    build_feed();
    ESP_LOGI(TAG, "News screen logic initialized");
}

static void logic_deinit(void)
{
    card_feed_detach(s_feed);   // лента (и её карточки) остаётся жить в скрытом контейнере
}

lv_obj_t *screen_news_create(void)
{
    ui_ScreenNews_screen_init();
    logic_init();
    return ui_ScreenNews;
}

void screen_news_destroy(lv_obj_t *scr)
{
    (void)scr;                        // экран - синглтон, его указатель лежит в ui_ScreenNews
    logic_deinit();                   // 1) подписки  2) таймеры  3) указатели на виджеты
    ui_ScreenNews_screen_destroy();     // lv_obj_del + обнуление глобальных ui_*
}
