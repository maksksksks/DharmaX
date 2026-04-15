#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c.h"
#include "bsp_gyro.h"

typedef struct {
    float accel_x, accel_y, accel_z; // В единицах g
    float gyro_x, gyro_y, gyro_z;   // В градусах в секунду (°/s)
} drv_mpu6050_data_t;

void drv_mpu6050_init(i2c_master_bus_handle_t i2c_bus_handle);
bool drv_mpu6050_read_batch(uint16_t max_packets, drv_mpu6050_data_t *out_data, uint16_t *out_count);