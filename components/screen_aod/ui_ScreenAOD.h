#ifndef UI_SCREENAOD_H
#define UI_SCREENAOD_H

#ifdef __cplusplus
extern "C" {
#endif

extern void ui_ScreenAOD_screen_init(void);
extern void ui_ScreenAOD_screen_destroy(void);
extern void ui_ScreenAOD_full_init(void);
extern void ui_event_ScreenAOD(lv_event_t * e);

extern lv_obj_t * ui_ScreenAOD;
extern lv_obj_t * ui_AodHourHand;
extern lv_obj_t * ui_AodMinuteHand;
extern lv_obj_t * ui_AodSecondDot;
extern lv_obj_t * ui_AodRing;

#define AOD_HAND_WIDTH        20
#define AOD_HOUR_HAND_LEN     58
#define AOD_MINUTE_HAND_LEN   92
#define AOD_SECOND_TRACK_R    90
#define AOD_SECOND_DOT_SIZE   20

#define EXAMPLE_LCD_H_RES           280
#define EXAMPLE_LCD_V_RES           456

#ifdef __cplusplus
}
#endif

#endif