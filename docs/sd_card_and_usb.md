# SD-карта и USB на ESP32-S3-Touch-AMOLED-1.64

## 1. Монтирование карты (нужно для `screen_sd`)

На плате Waveshare ESP32-S3-Touch-AMOLED-1.64 карта может быть подключена
либо по SPI, либо по SDMMC (1-bit). В их собственных примерах это переключается
макросом `SD_Read_Mode` в `sd_card_bsp.c` - то есть выбор режима зависит от
того, как разведена именно ваша плата/ревизия. Уточните по своей схеме, какие
пины уходят на TF-слот, и используйте соответствующий вариант ниже.

### Вариант А - SDMMC 1-bit (обычно быстрее, меньше пинов заняты корректно)

```c
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"

void mount_sd_card(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1; // 1-bit режим

    // Подставьте реальные пины CLK/CMD/D0 с вашей платы:
    slot_config.clk = GPIO_NUM_NC;
    slot_config.cmd = GPIO_NUM_NC;
    slot_config.d0  = GPIO_NUM_NC;

    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config,
                                            &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE("SD", "Не удалось смонтировать карту (0x%x)", ret);
        return;
    }
    sdmmc_card_print_info(stdout, card);
}
```

### Вариант Б - SPI

```c
#include "esp_vfs_fat.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"

void mount_sd_card(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = GPIO_NUM_NC, // подставьте реальные пины
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = GPIO_NUM_NC,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_NUM_NC; // подставьте реальный CS
    slot_config.host_id = host.slot;

    sdmmc_card_t *card;
    esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);
}
```

`screen_sd_logic.c` (проводник) уже написан против точки монтирования
`/sdcard` - как только один из вариантов выше отработает при старте (до
`ui_init()`/`screen_sd_logic_init()`), список файлов появится сам.

## 2. Можно ли читать/писать SD-карту с компьютера через USB?

**Коротко: да, технически можно** - ESP32-S3 (в т.ч. на этой плате) имеет
встроенный USB OTG (native USB), физически выведенный на тот же Type-C, через
который вы прошиваете часы. Через него ESP-IDF может поднять **USB MSC (Mass
Storage Class)** и отдать SD-карту компьютеру как обычную флешку - для этого
есть готовый компонент `esp_tinyusb` с примером `storage_msc_sdmmc` /
`tusb_msc` в `esp-idf/examples/peripherals/usb/device/`.

**Но есть важные ограничения, которые стоит учитывать:**

1. **Один USB-порт - один режим за раз.** По умолчанию Type-C на этой плате
   используется как USB-Serial/JTAG (прошивка + `idf.py monitor`). Чтобы
   отдать SD как MSC-диск, нужно в `menuconfig` переключить USB-стек на
   TinyUSB (`Component config → USB Host/Device → ...`). Одновременно
   получить и лог по USB, и MSC можно, но только через **композитный
   USB-девайс** (TinyUSB CDC+MSC) - это отдельная настройка, стандартный
   `idf.py monitor` через USB-Serial/JTAG тогда работать не будет (либо
   логируйте через отдельный UART).

2. **Эксклюзивный доступ к карте.** Пока часы сами используют SD (пишут
   логи, читают файлы для `screen_sd`), MSC-хосту (компьютеру) карту отдавать
   нельзя - будет гонка за FAT-таблицу и повреждение данных. Нужна
   арбитраж-логика: например, кнопка/пункт в `screen_sd` "Подключить как
   диск", которая размонтирует FATFS на стороне ESP32
   (`esp_vfs_fat_sdcard_unmount`), инициализирует TinyUSB MSC поверх того же
   `sdmmc_card_t`, и по физическому отключению USB - обратно монтирует для
   часов.

3. **Минимальный скелет** (после `mount_sd_card()` выше, при выборе "USB
   Mode" в UI):

```c
#include "tinyusb.h"
#include "tusb_msc_storage.h"

void start_usb_msc_mode(sdmmc_card_t *card)
{
    esp_vfs_fat_sdcard_unmount("/sdcard", card); // отдаём карту MSC-стеку

    const tinyusb_msc_sdmmc_config_t config_sdmmc = { .card = card };
    tinyusb_msc_storage_init_sdmmc(&config_sdmmc);

    tinyusb_config_t tusb_cfg = { 0 }; // дефолтные дескрипторы
    tinyusb_driver_install(&tusb_cfg);
}
```

Так что ответ практический: да, компьютер сможет читать/писать карту прямо
через кабель, но это отдельный режим ("USB-диск"), а не постоянный фоновый
доступ, и требует своего пункта UI + компонента `esp_tinyusb`
(`idf.py add-dependency "espressif/esp_tinyusb"`).
