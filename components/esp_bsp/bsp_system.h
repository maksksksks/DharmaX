#ifndef __BSP_SYSTEM_H__
#define __BSP_SYSTEM_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bsp_system {
    struct mpu6050 {
        int accelx, accely, accelz;
        int gyrox, gyroy, gyroz;
        int temp;
    } mpu6050;
    char* chip_model;
    int cpu_freq;
    int cpu_temp;

    int ram_size;
    int psram_size;
    int flash_size;

    int battery_voltage;
    int battery_percent;

    int photores;
    int brightness;

    int ds18;
} bsp_system;

bsp_system SystemInit();

#ifdef __cplusplus
}
#endif

#endif