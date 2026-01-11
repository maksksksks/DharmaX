#include "bsp_system.h"


bsp_system SystemInit() {
    bsp_system sys = {0};
    sys.chip_model = "ESP32-S3";
    return sys;
}