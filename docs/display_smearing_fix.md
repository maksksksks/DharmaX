# Артефакты/смазывание при обновлении экрана - причина и фикс

## Причина

В вашем `main.c`:

```c
const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
    EXAMPLE_PIN_NUM_LCD_CS,
    NULL,   // ← callback не нужен
    NULL);  // ← user_ctx не нужен
```

и

```c
static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_map)
{
    ...
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    lv_display_flush_ready(disp);   // ← вызывается СРАЗУ
}
```

`esp_lcd_panel_draw_bitmap()` для QSPI-панели не блокируется до конца физической
передачи - она ставит DMA-транзакцию в очередь SPI-драйвера и возвращает
управление сразу же. Реальная передача идёт в фоне.

Вызывая `lv_display_flush_ready()` немедленно, вы говорите LVGL "буфер
свободен, можно рисовать туда следующий кадр" - хотя QSPI ещё физически
досылает предыдущий. LVGL тут же начинает писать новые пиксели поверх
буфера, который ещё не долетел до экрана. Результат - ровно то, что вы
видите: "смазывание" именно в тех местах, где что-то обновилось (статичные
области никто не трогает - там всё чисто).

Комментарий в коде "*в LVGL 9 не нужен отдельный callback*" - это ошибочное
предположение. В LVGL 9 просто изменилось API вызова (`lv_display_flush_ready`
вместо `lv_disp_flush_ready`), но необходимость ждать реального завершения
DMA-передачи никуда не делась и не зависит от версии LVGL - это особенность
`esp_lcd`, а не LVGL.

## Фикс

Использовать `on_color_trans_done` - коллбэк, который `esp_lcd` вызывает
из ISR ровно в момент, когда SPI/QSPI транзакция реально завершилась.

### 1. Добавить статический указатель на дисплей и коллбэк

```c
static lv_display_t *s_disp = NULL;   // добавить рядом с lvgl_mux

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                             esp_lcd_panel_io_event_data_t *edata,
                                             void *user_ctx)
{
    if (s_disp) {
        lv_display_flush_ready(s_disp);
    }
    return false;
}
```

### 2. Убрать `lv_display_flush_ready()` из `flush_cb`

```c
static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    const int offsetx1 = area->x1 + 0x14;
    const int offsetx2 = area->x2 + 0x14;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    // lv_display_flush_ready() теперь вызывается из example_notify_lvgl_flush_ready()
    // (реальное завершение QSPI-передачи), а не отсюда.
}
```

### 3. Подключить коллбэк вместо `NULL`

```c
const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
    EXAMPLE_PIN_NUM_LCD_CS,
    example_notify_lvgl_flush_ready,   // было NULL
    NULL);
```

### 4. Установить `s_disp` сразу после создания дисплея

```c
lv_display_t *disp = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
s_disp = disp;   // добавить эту строку
lv_display_set_flush_cb(disp, example_lvgl_flush_cb);
...
```

Больше ничего менять не нужно - буферы, `RENDER_MODE_PARTIAL`, высота
буфера 40 строк - всё это нормально и не имеет отношения к артефактам,
это (совершенно правильно уже настроенная) оптимизация памяти под PARTIAL
режим. Проблема была именно в синхронизации, не в размере буфера.

## Второй момент, который стоит проверить, если первого фикса не хватит

Если после фикса выше артефакты остались (маловероятно, но на всякий
случай) - попробуйте снизить частоту QSPI-шины. В вашей конфигурации она
берётся из макроса `SH8601_PANEL_BUS_QSPI_CONFIG` по умолчанию; на длинном
шлейфе/проводах между модулем и платой высокая частота может давать
битовые ошибки на линии, которые выглядят как "мусор" именно в свежих
кадрах (аналогичный кейс на форуме ESP32: понижение `pclk_hz` с 10 МГц до
5 МГц полностью убрало артефакты на похожей связке). Но начните с фикса
`on_color_trans_done` - в 95% случаев именно в нём дело.
