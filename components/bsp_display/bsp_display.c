#include "bsp_display.h"
#include "esp_log.h"

static const char *TAG = "BSP_DISPLAY";
static esp_lcd_panel_io_handle_t s_bl_io_handle = NULL;

void bsp_display_backlight_attach(esp_lcd_panel_io_handle_t io_handle)
{
    s_bl_io_handle = io_handle;
}

void bsp_display_set_brightness(uint8_t percent)
{
    if (s_bl_io_handle == NULL) {
        ESP_LOGW(TAG, "backlight not attached yet - call bsp_display_backlight_attach() first");
        return;
    }
    if (percent > 100) percent = 100;

    uint8_t raw = (uint8_t)((uint32_t)percent * 255 / 100);

    // Та же последовательность, что была в main.c::setBrightnes() -
    // команда 0x51 (Write Display Brightness) для SH8601.
    uint32_t lcd_cmd = 0x51;
    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= 0x02 << 24;

    esp_lcd_panel_io_tx_param(s_bl_io_handle, lcd_cmd, &raw, 1);
}
