#pragma once
#include "esp_lcd_panel_io.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Вызвать один раз из main.c сразу после esp_lcd_new_panel_io_spi(),
// передав получившийся io_handle - до этого bsp_display_set_brightness()
// не будет иметь эффекта.
void bsp_display_backlight_attach(esp_lcd_panel_io_handle_t io_handle);

// percent: 0-100. Внутри конвертируется в диапазон регистра 0-255 (команда 0x51).
// Это прямая замена вашей старой setBrightnes(uint8_t brig 0-255) из main.c -
// svc_auto_brightness.c уже вызывает именно эту функцию с процентами.
void bsp_display_set_brightness(uint8_t percent);

#ifdef __cplusplus
} /*extern "C"*/
#endif
