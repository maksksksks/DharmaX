#include "notifications_tile.h"
#include "app_events.h"

// ★ Объявляем наш шрифт с кириллицей
LV_FONT_DECLARE(lv_font_montserrat_14_cyrillic)

static lv_obj_t * obj_notifications_tile = NULL;
lv_obj_t * ui_LabelNotifyHeader = NULL;
lv_obj_t * ui_ContainerNotify = NULL;

// Структура для передачи данных в таймер LVGL
typedef struct {
    lv_obj_t * card;
    lv_timer_t * timer;
} notify_auto_delete_t;

// Коллбэк таймера автоудаления
static void notify_auto_delete_cb(lv_timer_t * timer) {
    notify_auto_delete_t * nd = (notify_auto_delete_t *)timer->user_data;
    
    // Удаляем карточку со всеми дочерними элементами
    if (nd->card) lv_obj_del(nd->card);
    
    // Удаляем сам таймер (он удалится сам после возврата, но лучше сделать явно)
    lv_timer_del(timer);
    
    // Освобождаем память
    lv_mem_free(nd);
}

void add_notification_to_display(notify_data notify) {
    // 1. Создаем карточку
    lv_obj_t * card = lv_obj_create(ui_ContainerNotify);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(90));
    lv_obj_set_height(card, LV_SIZE_CONTENT); // Высота по содержимому
    lv_obj_set_align(card, LV_ALIGN_TOP_MID);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
    
    // ★ Тёмная тема карточки: тёмно-серый фон, чтобы отличаться от черного фона экрана
    lv_obj_set_style_radius(card, 8, LV_PART_MAIN | LV_STATE_DEFAULT); // Чуть более скругленные углы
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1C1C1C), LV_PART_MAIN | LV_STATE_DEFAULT); // Тёмно-серый
    lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN | LV_STATE_DEFAULT); // Отступы внутри карточки
    lv_obj_set_style_pad_row(card, 4, LV_PART_MAIN | LV_STATE_DEFAULT);  // Отступ между строками текста
    
    // Flex для расположения текста друг под другом
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // 2. Заголовок (Имя приложения)
    lv_obj_t * lbl_app = lv_label_create(card);
    lv_label_set_text(lbl_app, notify.app);
    // ★ Тёмная тема: яркий белый цвет для имени приложения
    lv_obj_set_style_text_color(lbl_app, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_app, &lv_font_montserrat_14_cyrillic, 0); 

    // 3. Текст уведомления
    lv_obj_t * lbl_data = lv_label_create(card);
    lv_label_set_text(lbl_data, notify.data);
    lv_obj_set_width(lbl_data, lv_pct(100)); // Занимает всю ширину карточки
    lv_label_set_long_mode(lbl_data, LV_LABEL_LONG_WRAP); // Перенос строк!
    // ★ Тёмная тема: приглушенный белый (светло-серый) для текста, чтобы не резал глаза
    lv_obj_set_style_text_color(lbl_data, lv_color_hex(0xB0B0B0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_data, &lv_font_montserrat_14_cyrillic, 0);

    // 4. Таймер самоудаления (10 минут = 600 000 мс)
    notify_auto_delete_t * nd = lv_mem_alloc(sizeof(notify_auto_delete_t));
    nd->card = card;
    nd->timer = lv_timer_create(notify_auto_delete_cb, 600000, nd);
    
    // Настройка скролла контейнера
    lv_obj_add_flag(ui_ContainerNotify, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui_ContainerNotify, LV_OBJ_FLAG_SCROLL_ELASTIC);
    
    // Автоматическая прокрутка вниз при новом уведомлении (чтобы было видно новое)
    lv_obj_scroll_to_y(ui_ContainerNotify, LV_COORD_MAX, LV_ANIM_ON);
}

static void lv_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == EVT_BLE_NOTIFY_RX) {
        notify_data *n_data = (notify_data *)event_data;
        add_notification_to_display(*n_data);
    }
}

void notifications_tile_init(lv_obj_t *parent) {
    obj_notifications_tile = parent;

    // ★ Тёмная тема экрана: чистый черный (экономит батарею на AMOLED)
    lv_obj_set_style_bg_color(obj_notifications_tile, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj_notifications_tile, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Заголовок "Notifications"
    ui_LabelNotifyHeader = lv_label_create(obj_notifications_tile);
    lv_obj_set_width(ui_LabelNotifyHeader, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_LabelNotifyHeader, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_LabelNotifyHeader, 0);
    lv_obj_set_y(ui_LabelNotifyHeader, -144);
    lv_obj_set_align(ui_LabelNotifyHeader, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelNotifyHeader, "Notifications");
    // ★ Тёмная тема: белый текст для заголовка
    lv_obj_set_style_text_color(ui_LabelNotifyHeader, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelNotifyHeader, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Контейнер для списка уведомлений
    ui_ContainerNotify = lv_obj_create(obj_notifications_tile);
    lv_obj_remove_style_all(ui_ContainerNotify);
    lv_obj_set_width(ui_ContainerNotify, lv_pct(100));
    lv_obj_set_height(ui_ContainerNotify, lv_pct(88));
    lv_obj_set_x(ui_ContainerNotify, 0);
    lv_obj_set_y(ui_ContainerNotify, 19);
    lv_obj_set_align(ui_ContainerNotify, LV_ALIGN_CENTER);
    // ★ Тёмная тема: прозрачный фон контейнера (чтобы просвечивал черный экран)
    lv_obj_set_style_bg_opa(ui_ContainerNotify, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_set_flex_flow(ui_ContainerNotify, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_ContainerNotify, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // ★ Отступ между карточками уведомлений
    lv_obj_set_style_pad_row(ui_ContainerNotify, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_clear_flag(ui_ContainerNotify, LV_OBJ_FLAG_CLICKABLE);
    // Скролл включаем программно в add_notification_to_display

    esp_event_handler_instance_register(APP_EVENTS, EVT_BLE_NOTIFY_RX, lv_event_handler, NULL, NULL);
}