// Hand-written screen (в SquareLine Studio не рисовался, экран пульта строится
// в коде) - живёт здесь же, рядом с остальными ui_ScreenX.h, чтобы ui.h мог
// подключить его как любой другой экран.

#ifndef UI_SCREENIR_H
#define UI_SCREENIR_H

#ifdef __cplusplus
extern "C" {
#endif

// SCREEN: ui_ScreenIR
extern void ui_ScreenIR_screen_init(void);
extern void ui_ScreenIR_screen_destroy(void);
extern void ui_event_ScreenIR(lv_event_t * e);
extern lv_obj_t * ui_ScreenIR;
extern lv_obj_t * ui_IRTabView;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
