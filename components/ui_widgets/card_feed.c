#include "card_feed.h"
#include "ui_theme.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

LV_FONT_DECLARE(lv_font_montserrat_26_cyrillic)

#define SWIPE_START_THRESHOLD 10
#define SWIPE_DELETE_THRESHOLD 80
#define SWIPE_VELOCITY_THRESHOLD 5

struct card_feed_s {
    lv_obj_t *container;
    uint32_t auto_delete_ms;
    lv_obj_t *active_swipe_card;
};

typedef struct {
    card_feed_t feed;
    char id[32];
    char grp[32];
    lv_timer_t *auto_delete_timer;
} card_user_data_t;

// ==========================================
// УДАЛЕНИЕ
// ==========================================
static void delete_card(card_feed_t feed, lv_obj_t *card)
{
    if (!card) return;

    if (feed->active_swipe_card == card) {
        feed->active_swipe_card = NULL;
        lv_obj_add_flag(feed->container, LV_OBJ_FLAG_SCROLLABLE);
    }

    card_user_data_t *ud = (card_user_data_t *)lv_obj_get_user_data(card);
    if (ud) {
        if (ud->auto_delete_timer) lv_timer_del(ud->auto_delete_timer);
        lv_free(ud);
    }
    lv_obj_delete(card);
}

static void auto_delete_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *card = (lv_obj_t *)lv_timer_get_user_data(timer);
    if (!card) return;
    card_user_data_t *ud = (card_user_data_t *)lv_obj_get_user_data(card);
    if (ud) delete_card(ud->feed, card);
}

// ==========================================
// АНИМАЦИИ СВАЙПА
// ==========================================
static void anim_x_cb(void *obj, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)obj, v, 0);
}

static void swipe_delete_ready_cb(lv_anim_t *a)
{
    lv_obj_t *card = (lv_obj_t *)a->var;
    card_user_data_t *ud = (card_user_data_t *)lv_obj_get_user_data(card);
    if (ud) delete_card(ud->feed, card);
}

static void snap_back_ready_cb(lv_anim_t *a)
{
    lv_obj_t *card = (lv_obj_t *)a->var;
    card_user_data_t *ud = (card_user_data_t *)lv_obj_get_user_data(card);
    if (ud && ud->feed) lv_obj_add_flag(ud->feed->container, LV_OBJ_FLAG_SCROLLABLE);
}

// ==========================================
// ЖЕСТЫ
// ==========================================
static void card_pressing_cb(lv_event_t *e)
{
    lv_obj_t *card = lv_event_get_target(e);
    card_user_data_t *ud = (card_user_data_t *)lv_obj_get_user_data(card);
    if (!ud) return;
    card_feed_t feed = ud->feed;

    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    if (feed->active_swipe_card == card ||
        (abs(vect.x) > abs(vect.y) && abs(vect.x) > SWIPE_START_THRESHOLD)) {
        if (feed->active_swipe_card == NULL) {
            feed->active_swipe_card = card;
            lv_obj_remove_flag(feed->container, LV_OBJ_FLAG_SCROLLABLE);
        }
        int32_t cur_x = lv_obj_get_style_translate_x(card, 0);
        lv_obj_set_style_translate_x(card, cur_x + vect.x, 0);
    }
}

static void card_released_cb(lv_event_t *e)
{
    lv_obj_t *card = lv_event_get_target(e);
    card_user_data_t *ud = (card_user_data_t *)lv_obj_get_user_data(card);
    if (!ud) return;
    card_feed_t feed = ud->feed;

    if (feed->active_swipe_card != card) return;
    feed->active_swipe_card = NULL;

    int32_t cur_x = lv_obj_get_style_translate_x(card, 0);

    lv_indev_t *indev = lv_indev_active();
    lv_point_t vect = {0, 0};
    if (indev) lv_indev_get_vect(indev, &vect);

    bool should_delete = (abs(cur_x) > SWIPE_DELETE_THRESHOLD) || (abs(vect.x) > SWIPE_VELOCITY_THRESHOLD);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, card);
    lv_anim_set_exec_cb(&a, anim_x_cb);

    if (should_delete) {
        int32_t target_x = (cur_x > 0) ? 300 : -300;
        lv_anim_set_values(&a, cur_x, target_x);
        lv_anim_set_time(&a, 250);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_ready_cb(&a, swipe_delete_ready_cb);
    } else {
        lv_anim_set_values(&a, cur_x, 0);
        lv_anim_set_time(&a, 300);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_ready_cb(&a, snap_back_ready_cb);
    }
    lv_anim_start(&a);
}

// ==========================================
// СОЗДАНИЕ ЛЕНТЫ / КАРТОЧЕК
// ==========================================
card_feed_t card_feed_create(lv_obj_t *parent, uint32_t auto_delete_ms)
{
    card_feed_t feed = lv_malloc(sizeof(struct card_feed_s));
    memset(feed, 0, sizeof(*feed));
    feed->auto_delete_ms = auto_delete_ms;

    feed->container = lv_obj_create(parent);
    lv_obj_remove_style_all(feed->container);
    lv_obj_set_size(feed->container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(feed->container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(feed->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(feed->container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(feed->container, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_top(feed->container, 6, LV_PART_MAIN);
    lv_obj_add_flag(feed->container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(feed->container, LV_OBJ_FLAG_SCROLL_ELASTIC);

    return feed;
}

void card_feed_add(card_feed_t feed, const card_feed_item_t *item)
{
    if (!feed) return;

    lv_obj_t *card = lv_obj_create(feed->container);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(96));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_align(card, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(UI_COLOR_SURFACE_DIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(UI_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, 6, LV_PART_MAIN);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    if ((item->header_left && item->header_left[0]) || (item->header_right && item->header_right[0])) {
        lv_obj_t *row_top = lv_obj_create(card);
        lv_obj_remove_style_all(row_top);
        lv_obj_set_width(row_top, lv_pct(100));
        lv_obj_set_height(row_top, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row_top, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        if (item->header_left && item->header_left[0]) {
            lv_obj_t *lbl = lv_label_create(row_top);
            lv_label_set_text(lbl, item->header_left);
            lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_ACCENT), LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_26_cyrillic, 0);
        }
        if (item->header_right && item->header_right[0]) {
            lv_obj_t *lbl = lv_label_create(row_top);
            lv_label_set_text(lbl, item->header_right);
            lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_26_cyrillic, 0);
        }
    }

    if (item->title && item->title[0]) {
        lv_obj_t *lbl = lv_label_create(card);
        lv_label_set_text(lbl, item->title);
        lv_obj_set_width(lbl, lv_pct(100));
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_26_cyrillic, 0);   // было montserrat_14
    }

    if (item->body && item->body[0]) {
        lv_obj_t *lbl = lv_label_create(card);
        lv_label_set_text(lbl, item->body);
        lv_obj_set_width(lbl, lv_pct(100));
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_26_cyrillic, 0);   // было montserrat_14
    }

    card_user_data_t *ud = lv_malloc(sizeof(card_user_data_t));
    memset(ud, 0, sizeof(*ud));
    ud->feed = feed;
    if (item->id) snprintf(ud->id, sizeof(ud->id), "%s", item->id);
    if (item->grp) snprintf(ud->grp, sizeof(ud->grp), "%s", item->grp);
    lv_obj_set_user_data(card, ud);

    if (feed->auto_delete_ms > 0) {
        ud->auto_delete_timer = lv_timer_create(auto_delete_timer_cb, feed->auto_delete_ms, card);
    }

    lv_obj_add_event_cb(card, card_pressing_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(card, card_released_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_scroll_to_y(feed->container, LV_COORD_MAX, LV_ANIM_ON);
}

void card_feed_remove_by_id(card_feed_t feed, const char *id)
{
    if (!feed || !id) return;
    uint32_t cnt = lv_obj_get_child_count(feed->container);
    for (int i = (int)cnt - 1; i >= 0; i--) {
        lv_obj_t *child = lv_obj_get_child(feed->container, i);
        card_user_data_t *ud = (card_user_data_t *)lv_obj_get_user_data(child);
        if (ud && strcmp(ud->id, id) == 0) {
            delete_card(feed, child);
            return;
        }
    }
}

void card_feed_remove_by_group(card_feed_t feed, const char *grp)
{
    if (!feed || !grp || !grp[0]) return;
    uint32_t cnt = lv_obj_get_child_count(feed->container);
    for (int i = (int)cnt - 1; i >= 0; i--) {
        lv_obj_t *child = lv_obj_get_child(feed->container, i);
        card_user_data_t *ud = (card_user_data_t *)lv_obj_get_user_data(child);
        if (ud && strcmp(ud->grp, grp) == 0) {
            delete_card(feed, child);
        }
    }
}

void card_feed_clear(card_feed_t feed)
{
    if (!feed) return;
    uint32_t cnt = lv_obj_get_child_count(feed->container);
    for (int i = (int)cnt - 1; i >= 0; i--) {
        delete_card(feed, lv_obj_get_child(feed->container, i));
    }
}

lv_obj_t *card_feed_get_container(card_feed_t feed)
{
    return feed ? feed->container : NULL;
}
