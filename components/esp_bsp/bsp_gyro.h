#ifndef __BSP_MPU6050_H__
#define __BSP_MPU6050_H__

#include <stdio.h>
#include <stdint.h>
#include "driver/i2c.h"
#include "bsp_i2c.h"

#define MPU6050_I2C_ADDR         0x69 // Убедись, что AD0 подтянут к GND (0x68) или VCC (0x69)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
} bsp_mpu6050_raw_data_t;

// Передаем хэндл шины I2C из bsp_i2c_init()
void bsp_mpu6050_init(i2c_master_bus_handle_t i2c_bus_handle);
uint16_t bsp_mpu6050_get_fifo_count(void);
esp_err_t bsp_mpu6050_read_fifo(uint16_t num_packets, bsp_mpu6050_raw_data_t *data_out);
void bsp_mpu6050_reset_fifo(void);

#ifdef __cplusplus
}
#endif

#endif