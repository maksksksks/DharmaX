// Hand-written screen (SquareLine Studio can't build custom-drawn watchfaces),
// kept in the same style/location as the generated ui_ScreenX.h files so that
// ui.h can aggregate it like any other screen.

#ifndef UI_SCREENAOD_H
#define UI_SCREENAOD_H

#ifdef __cplusplus
extern "C" {
#endif

// SCREEN: ui_ScreenAOD
extern void ui_ScreenAOD_screen_init(void);
extern void ui_ScreenAOD_screen_destroy(void);
extern void ui_ScreenAOD_full_init(void);
extern void ui_event_ScreenAOD(lv_event_t * e);
extern lv_obj_t * ui_ScreenAOD;

// CUSTOM VARIABLES (используются в screen_aod_logic.c для обновления стрелок)
extern lv_obj_t * ui_AodHourHand;
extern lv_obj_t * ui_AodMinuteHand;
extern lv_obj_t * ui_AodSecondDot;
extern lv_obj_t * ui_AodRing;

// Геометрия, нужна и в ui_ScreenAOD.c (построение), и в screen_aod_logic.c (обновление)
#define AOD_HAND_WIDTH         5    // толщина стрелок = диаметр секундной точки
#define AOD_HOUR_HAND_LEN     58    // длина часовой стрелки (короче)
#define AOD_MINUTE_HAND_LEN   92    // длина минутной стрелки (длиннее)
#define AOD_SECOND_TRACK_R   100    // радиус окружности, по которой бегает точка секунд

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
