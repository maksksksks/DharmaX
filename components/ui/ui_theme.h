#pragma once
// Единая палитра часов. Тёмная/AMOLED-экономная: фон чистый чёрный (пиксели
// выключены), акцент - светлый, почти белый, лавандовый.
//
// Использование: #include "ui.h" уже тянет этот файл во все экраны.

#define UI_COLOR_BG              0x000000  // фон экранов - чистый чёрный (economy)
#define UI_COLOR_SURFACE         0x18141F  // поверхность карточек/панелей
#define UI_COLOR_SURFACE_ALT     0x221C2E  // чуть светлее - выделенные/hover поверхности
#define UI_COLOR_BORDER          0x362E45  // тонкие разделители/обводки

#define UI_COLOR_ACCENT          0xD0BCFF  // основной акцент (лавандовый, почти белый)
#define UI_COLOR_ACCENT_DIM      0x8A7AAE  // приглушённый акцент (неактивные элементы/иконки)
#define UI_COLOR_ACCENT_PRESSED  0xB79CFF  // акцент в нажатом состоянии

#define UI_COLOR_TEXT_PRIMARY    0xF3EEFF  // основной текст - почти белый с лавандовым оттенком
#define UI_COLOR_TEXT_SECONDARY  0x9C93B5  // вторичный текст (подписи, метки)
#define UI_COLOR_TEXT_DISABLED   0x554F66

#define UI_COLOR_DANGER          0xFF6B6B  // удаление/ошибка/разряд батареи
#define UI_COLOR_SUCCESS         0xB9F6CA  // подтверждение/ОК

// Утилита: применить "базовый" стиль экрана (чёрный фон) одной строкой.
#define UI_APPLY_SCREEN_BG(obj) \
    do { \
        lv_obj_set_style_bg_color((obj), lv_color_hex(UI_COLOR_BG), LV_PART_MAIN | LV_STATE_DEFAULT); \
        lv_obj_set_style_bg_opa((obj), 255, LV_PART_MAIN | LV_STATE_DEFAULT); \
    } while (0)
