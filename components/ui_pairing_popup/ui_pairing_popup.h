#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Портировано со старой платы (там жило в lvgl_ui.c), адаптировано под
// новую тему. main.c уже вызывает эти функции по именам - только не
// хватало их реализации в новом проекте.

void show_pairing_popup(const char *code);
void close_pairing_popup_cb(lv_event_t *e); // e может быть NULL, не используется
bool get_popup_status(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
