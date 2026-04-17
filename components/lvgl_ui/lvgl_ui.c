#include "lvgl_ui.h"
#include "tileview/system_tile.h"
#include "rgb_tile.h"
#include "home_tile.h"
#include "settings_tile.h"
#include "notifications_tile.h"
#include "app_events.h"
// #include "tileview/wifi_tile.h"
// #include "tileview/ble_tile.h"

static bool flagPopup = false;


void lvgl_ui_init(void) {
    lv_obj_t *tileview = lv_tileview_create(lv_scr_act());
    lv_obj_set_style_bg_opa(tileview, LV_OPA_0, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(tileview, LV_OPA_0, LV_PART_SCROLLBAR | LV_STATE_SCROLLED);

    lv_obj_t *rgb_tile = lv_tileview_add_tile(tileview, 0, 1, LV_DIR_HOR);
    rgb_tile_init(rgb_tile);

    lv_obj_t *home_tile = lv_tileview_add_tile(tileview, 1, 1, LV_DIR_ALL);
    home_tile_init(home_tile);

    lv_obj_t *system_tile = lv_tileview_add_tile(tileview, 2, 1, LV_DIR_HOR);
    system_tile_init(system_tile);

    lv_obj_t *settings_tile = lv_tileview_add_tile(tileview, 1, 2, LV_DIR_VER);
    settings_tile_init(settings_tile);

    lv_obj_t *notifications_tile = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_VER);    
    notifications_tile_init(notifications_tile);
    // lv_obj_t *wifi_tile = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    // wifi_tile_init(wifi_tile);

    // lv_obj_t *ble_tile = lv_tileview_add_tile(tileview, 4, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    // ble_tile_init(ble_tile);
}

// Глобальный или статический указатель, чтобы можно было закрыть окно
static lv_obj_t * pairing_popup_bg = NULL;

// Функция закрытия попапа
void close_pairing_popup_cb(lv_event_t * e) {
    if (pairing_popup_bg != NULL) {
        flagPopup = false;
        lv_obj_del(pairing_popup_bg);
        pairing_popup_bg = NULL;
    }
}

// Функция показа попапа
void show_pairing_popup(const char * code) {
    flagPopup = true;
    // Если окно уже открыто, ничего не делаем (или можно обновить код)
    if (pairing_popup_bg != NULL) return;

    // 1. Создаем полупрозрачный фон на ВЕСЬ экран (поверх tileview)
    pairing_popup_bg = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(pairing_popup_bg); // Убираем стандартный стиль
    lv_obj_set_size(pairing_popup_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(pairing_popup_bg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pairing_popup_bg, LV_OPA_60, 0); // Затемнение на 60%
    lv_obj_add_flag(pairing_popup_bg, LV_OBJ_FLAG_CLICKABLE); // Блокируем клики сквозь фон
    lv_obj_center(pairing_popup_bg);

    // 2. Создаем само окно (карточку) поверх фона
    lv_obj_t * popup_card = lv_obj_create(pairing_popup_bg);
    lv_obj_set_size(popup_card, 200, 150); // Задайте нужный размер
    lv_obj_center(popup_card);
    lv_obj_set_style_radius(popup_card, 15, 0); // Закругленные углы

    lv_obj_t * title = lv_label_create(popup_card);
    lv_label_set_text(title, "Code");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // 4. Сам код (большим шрифтом)
    lv_obj_t * code_label = lv_label_create(popup_card);
    lv_label_set_text(code_label, code);
    lv_obj_set_style_text_font(code_label, &lv_font_montserrat_28, 0); 
    lv_obj_center(code_label);
    lv_obj_set_y(code_label, 0); // Чуть ниже центра

    // 5. Кнопка закрытия (опционально, можно закрывать по таймеру)
    lv_obj_t * close_btn = lv_btn_create(popup_card);
    lv_obj_set_size(close_btn, 80, 35);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(close_btn, close_pairing_popup_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_label = lv_label_create(close_btn);
    lv_label_set_text(btn_label, "Close");
    lv_obj_center(btn_label);
}

bool get_popup_status() {
    return flagPopup;
}