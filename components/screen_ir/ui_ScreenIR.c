// Hand-written screen: экран-пульт для управления ИК-светодиодом.
// TabView с 3 вкладками (Samsung / TCL / Extra), в каждой - вертикальный
// скроллируемый список кнопок-строк (на экран помещается ~6 штук).

#include "ui.h"
#include "screen_ir_logic.h"

lv_obj_t * ui_ScreenIR = NULL;
lv_obj_t * ui_IRTabView = NULL;

// event funtions
// event funtions
void ui_event_ScreenIR(lv_event_t * e)
{
lv_event_code_t event_code = lv_event_get_code(e);
if (event_code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_LEFT) {
lv_indev_wait_release(lv_indev_active());
_ui_screen_change(&ui_ScreenSys, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0, &ui_ScreenSys_full_init);
}
}

// =========================================================
// Строка-кнопка пульта: иконка/символ слева + подпись, вся ширина вкладки.
// Высота и отступы подобраны так, чтобы на экран помещалось ~6 штук.
// =========================================================
typedef struct {
    ir_device_t device;
    ir_cmd_id_t cmd;
} ir_btn_user_data_t;

static void remote_row_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ir_btn_user_data_t * ud = (ir_btn_user_data_t *)lv_event_get_user_data(e);
    screen_ir_handle_button(ud->device, ud->cmd);
}

static void make_remote_row(lv_obj_t * parent, const char * symbol, const char * text,
                            ir_device_t device, ir_cmd_id_t cmd)
{
    lv_obj_t * row = lv_button_create(parent);
    lv_obj_set_width(row, lv_pct(94));
    lv_obj_set_height(row, 54);
    lv_obj_set_style_radius(row, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(row, lv_color_hex(UI_COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(row, lv_color_hex(UI_COLOR_SURFACE_ALT), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, lv_color_hex(UI_COLOR_BORDER), LV_PART_MAIN);

    ir_btn_user_data_t * ud = lv_malloc(sizeof(ir_btn_user_data_t));
    ud->device = device;
    ud->cmd = cmd;
    lv_obj_add_event_cb(row, remote_row_event_cb, LV_EVENT_CLICKED, ud);

    lv_obj_t * lbl_icon = lv_label_create(row);
    lv_label_set_text(lbl_icon, symbol);
    lv_obj_align(lbl_icon, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_set_style_text_color(lbl_icon, lv_color_hex(UI_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_20, LV_PART_MAIN);

    lv_obj_t * lbl_text = lv_label_create(row);
    lv_label_set_text(lbl_text, text);
    lv_obj_align(lbl_text, LV_ALIGN_LEFT_MID, 52, 0);
    lv_obj_set_style_text_color(lbl_text, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_text, &lv_font_montserrat_14, LV_PART_MAIN);
}

static lv_obj_t * make_tab_list(lv_obj_t * tab)
{
    lv_obj_t * list = lv_obj_create(tab);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_top(list, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(list, 6, LV_PART_MAIN);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);          // вертикальная прокрутка
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(list, LV_SCROLL_SNAP_NONE);
    return list;
}

// Стандартный набор кнопок для телевизора: вкл/выкл, громкость, каналы,
// источник, стрелки. 11 строк - на экране видно ~6, остальное - скроллом.
static void build_tv_tab(lv_obj_t * tab, ir_device_t device)
{
    lv_obj_t * list = make_tab_list(tab);
    make_remote_row(list, LV_SYMBOL_POWER, "Power",       device, IR_CMD_POWER);
    make_remote_row(list, LV_SYMBOL_VOLUME_MAX, "Vol +",  device, IR_CMD_VOL_UP);
    make_remote_row(list, LV_SYMBOL_VOLUME_MID, "Vol -",  device, IR_CMD_VOL_DOWN);
    make_remote_row(list, LV_SYMBOL_PLUS, "Channel +",    device, IR_CMD_CH_UP);
    make_remote_row(list, LV_SYMBOL_MINUS, "Channel -",   device, IR_CMD_CH_DOWN);
    make_remote_row(list, LV_SYMBOL_LIST, "Source",       device, IR_CMD_SOURCE);
    make_remote_row(list, LV_SYMBOL_UP, "Up",             device, IR_CMD_UP);
    make_remote_row(list, LV_SYMBOL_DOWN, "Down",         device, IR_CMD_DOWN);
    make_remote_row(list, LV_SYMBOL_LEFT, "Left",         device, IR_CMD_LEFT);
    make_remote_row(list, LV_SYMBOL_RIGHT, "Right",       device, IR_CMD_RIGHT);
    make_remote_row(list, LV_SYMBOL_OK, "OK",             device, IR_CMD_OK);
}

// Доп. пульт: просто 8 настраиваемых кнопок под любые устройства.
static void build_extra_tab(lv_obj_t * tab)
{
    lv_obj_t * list = make_tab_list(tab);
    make_remote_row(list, LV_SYMBOL_SETTINGS, "Button 1", IR_DEVICE_EXTRA, IR_CMD_EXTRA_1);
    make_remote_row(list, LV_SYMBOL_SETTINGS, "Button 2", IR_DEVICE_EXTRA, IR_CMD_EXTRA_2);
    make_remote_row(list, LV_SYMBOL_SETTINGS, "Button 3", IR_DEVICE_EXTRA, IR_CMD_EXTRA_3);
    make_remote_row(list, LV_SYMBOL_SETTINGS, "Button 4", IR_DEVICE_EXTRA, IR_CMD_EXTRA_4);
    make_remote_row(list, LV_SYMBOL_SETTINGS, "Button 5", IR_DEVICE_EXTRA, IR_CMD_EXTRA_5);
    make_remote_row(list, LV_SYMBOL_SETTINGS, "Button 6", IR_DEVICE_EXTRA, IR_CMD_EXTRA_6);
    make_remote_row(list, LV_SYMBOL_SETTINGS, "Button 7", IR_DEVICE_EXTRA, IR_CMD_EXTRA_7);
    make_remote_row(list, LV_SYMBOL_SETTINGS, "Button 8", IR_DEVICE_EXTRA, IR_CMD_EXTRA_8);
}

void ui_ScreenIR_screen_init(void)
{
    ui_ScreenIR = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_ScreenIR, LV_OBJ_FLAG_SCROLLABLE);
    UI_APPLY_SCREEN_BG(ui_ScreenIR);

    ui_IRTabView = lv_tabview_create(ui_ScreenIR);
    lv_tabview_set_tab_bar_size(ui_IRTabView, 44);
    lv_obj_set_size(ui_IRTabView, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(ui_IRTabView, lv_color_hex(UI_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_IRTabView, 255, LV_PART_MAIN);

    lv_obj_t * tab_bar = lv_tabview_get_tab_bar(ui_IRTabView);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(UI_COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_text_color(tab_bar, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_ITEMS);
    lv_obj_set_style_text_color(tab_bar, lv_color_hex(UI_COLOR_ACCENT), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(tab_bar, lv_color_hex(UI_COLOR_ACCENT), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_t * tab_samsung = lv_tabview_add_tab(ui_IRTabView, "Samsung");
    lv_obj_t * tab_tcl     = lv_tabview_add_tab(ui_IRTabView, "TCL");
    lv_obj_t * tab_extra   = lv_tabview_add_tab(ui_IRTabView, "Extra");
    UI_APPLY_SCREEN_BG(tab_samsung);
    UI_APPLY_SCREEN_BG(tab_tcl);
    UI_APPLY_SCREEN_BG(tab_extra);

    build_tv_tab(tab_samsung, IR_DEVICE_SAMSUNG);
    build_tv_tab(tab_tcl, IR_DEVICE_TCL);
    build_extra_tab(tab_extra);

    lv_obj_add_event_cb(ui_ScreenIR, ui_event_ScreenIR, LV_EVENT_ALL, NULL);
}

void ui_ScreenIR_screen_destroy(void)
{
    if (ui_ScreenIR) lv_obj_del(ui_ScreenIR);

    ui_ScreenIR = NULL;
    ui_IRTabView = NULL;
}
