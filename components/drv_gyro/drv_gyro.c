#include "drv_gyro.h"
#include "bsp_gyro.h"
#include "esp_log.h"

static char *TAG = "DRV_MPU6050";

// Чувствительность для ±4g: 8192 LSB/g
#define ACCEL_SENSITIVITY 8192.0f
// Чувствительность для ±500 °/s: 65.5 LSB/(°/s)
#define GYRO_SENSITIVITY 65.5f

void drv_mpu6050_init(i2c_master_bus_handle_t i2c_bus_handle) {
    bsp_mpu6050_init(i2c_bus_handle);
}

bool drv_mpu6050_read_batch(uint16_t max_packets, drv_mpu6050_data_t *out_data, uint16_t *out_count) {
    uint16_t fifo_count = bsp_mpu6050_get_fifo_count();
    uint16_t num_packets = fifo_count / 12;
    
    if (num_packets == 0) {
        *out_count = 0;
        return true;
    }

    if (num_packets > max_packets) num_packets = max_packets;

    bsp_mpu6050_raw_data_t raw_buf[num_packets];
    esp_err_t err = bsp_mpu6050_read_fifo(num_packets, raw_buf);
    
    if (err != ESP_OK) {
        bsp_mpu6050_reset_fifo();
        ESP_LOGW(TAG, "FIFO read failed, reset");
        *out_count = 0;
        return false;
    }

    for (int i = 0; i < num_packets; i++) {
        out_data[i].accel_x = (float)raw_buf[i].accel_x / ACCEL_SENSITIVITY;
        out_data[i].accel_y = (float)raw_buf[i].accel_y / ACCEL_SENSITIVITY;
        out_data[i].accel_z = (float)raw_buf[i].accel_z / ACCEL_SENSITIVITY;
        
        out_data[i].gyro_x = (float)raw_buf[i].gyro_x / GYRO_SENSITIVITY;
        out_data[i].gyro_y = (float)raw_buf[i].gyro_y / GYRO_SENSITIVITY;
        out_data[i].gyro_z = (float)raw_buf[i].gyro_z / GYRO_SENSITIVITY;
    }

    *out_count = num_packets;
    return true;
}