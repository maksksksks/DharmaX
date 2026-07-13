#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Устройства (вкладки экрана пульта)
typedef enum {
IR_DEVICE_SAMSUNG = 0,
IR_DEVICE_TCL,
IR_DEVICE_EXTRA,   // доп. пульт с 8 произвольными кнопками под разные устройства
IR_DEVICE_COUNT,
} ir_device_t;

// Команды. Для TV (Samsung/TCL) используются IR_CMD_POWER..IR_CMD_OK,
// для Extra - IR_CMD_EXTRA_1..IR_CMD_EXTRA_8.
typedef enum {
IR_CMD_POWER = 0,
IR_CMD_VOL_UP,
IR_CMD_VOL_DOWN,
IR_CMD_CH_UP,
IR_CMD_CH_DOWN,
IR_CMD_SOURCE,
IR_CMD_UP,
IR_CMD_DOWN,
IR_CMD_LEFT,
IR_CMD_RIGHT,
IR_CMD_OK,
IR_CMD_EXTRA_1,
IR_CMD_EXTRA_2,
IR_CMD_EXTRA_3,
IR_CMD_EXTRA_4,
IR_CMD_EXTRA_5,
IR_CMD_EXTRA_6,
IR_CMD_EXTRA_7,
IR_CMD_EXTRA_8,
IR_CMD_COUNT,
} ir_cmd_id_t;

// Инициализация: поднимает RMT TX (см. svc_ir_transmit) на заданном GPIO.
void screen_ir_logic_init(void);

// Вызывается из ui_ScreenIR.c по клику на кнопку.
void screen_ir_handle_button(ir_device_t device, ir_cmd_id_t cmd);

// Обёртка для ленивой инициализации: создаёт экран + запускает логику.
void ui_ScreenIR_full_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif