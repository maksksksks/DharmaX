#include "bsp_gyro.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static char *TAG = "BSP_MPU6050";
static i2c_master_dev_handle_t mpu_dev_handle = NULL;

#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_FIFO_EN      0x23
#define MPU6050_REG_USER_CTRL    0x6A
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_FIFO_COUNTH  0x72
#define MPU6050_REG_FIFO_R_W     0x74
#define MPU6050_REG_WHO_AM_I     0x75

static esp_err_t mpu_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_transmit(mpu_dev_handle, write_buf, 2, 100 / portTICK_PERIOD_MS);
}

static esp_err_t mpu_read_reg(uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(mpu_dev_handle, &reg, 1, data, len, 100 / portTICK_PERIOD_MS);
}

void bsp_mpu6050_init(i2c_master_bus_handle_t i2c_bus_handle) {
    if (mpu_dev_handle != NULL) return;

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_I2C_ADDR,
        .scl_speed_hz = 400000,
    };
    
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &mpu_dev_handle));
    ESP_LOGI(TAG, "Device added to I2C bus at addr 0x%02X", MPU6050_I2C_ADDR);

    // Даем время датчику на включение
    vTaskDelay(pdMS_TO_TICKS(100));

    // Проверяем WHO_AM_I
    uint8_t who_am_i = 0;
    esp_err_t err = mpu_read_reg(MPU6050_REG_WHO_AM_I, &who_am_i, 1);
    if (err != ESP_OK || who_am_i != 0x68) {
        ESP_LOGE(TAG, "❌ WHO_AM_I FAILED! Read: 0x%02X, Expected: 0x68, Err: %s", 
                 who_am_i, esp_err_to_name(err));
        ESP_LOGE(TAG, "Check wiring! SDA/SCL must match touch screen pins. Try address 0x69!");
        return; // Прерываем инициализацию, датчик мертв
    }
    ESP_LOGI(TAG, "✅ WHO_AM_I check passed (0x%02X)", who_am_i);

    // Аппаратный сброс датчика (обязательно для чистого старта)
    mpu_write_reg(MPU6050_REG_PWR_MGMT_1, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100)); // Ждем пока сбросится

    // Пробуждаем и ставим часы на Gyro PLL
    mpu_write_reg(MPU6050_REG_PWR_MGMT_1, 0x01); 
    vTaskDelay(pdMS_TO_TICKS(50));

    // 100 Hz
    mpu_write_reg(MPU6050_REG_SMPLRT_DIV, 0x09);
    // DLPF 44Hz Accel, 42Hz Gyro
    mpu_write_reg(MPU6050_REG_CONFIG, 0x03);
    // Гироскоп ±500 °/s
    mpu_write_reg(MPU6050_REG_GYRO_CONFIG, 0x08);
    // Акселерометр ±4g
    mpu_write_reg(MPU6050_REG_ACCEL_CONFIG, 0x08);
    
    // Сброс FIFO бита на всякий случай
    mpu_write_reg(MPU6050_REG_USER_CTRL, 0x44);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Включаем FIFO для осей Accel + Gyro
    mpu_write_reg(MPU6050_REG_FIFO_EN, 0x78);
    // Включаем FIFO
    mpu_write_reg(MPU6050_REG_USER_CTRL, 0x40);

    ESP_LOGI(TAG, "MPU6050 configured with FIFO");
}

uint16_t bsp_mpu6050_get_fifo_count(void) {
    uint8_t buf[2] = {0};
    mpu_read_reg(MPU6050_REG_FIFO_COUNTH, buf, 2);
    return ((uint16_t)buf[0] << 8) | buf[1];
}

void bsp_mpu6050_reset_fifo(void) {
    mpu_write_reg(MPU6050_REG_USER_CTRL, 0x44);
}

esp_err_t bsp_mpu6050_read_fifo(uint16_t num_packets, bsp_mpu6050_raw_data_t *data_out) {
    uint16_t bytes_to_read = num_packets * 12;
    
    // Статический буфер, чтобы не фрагментировать кучу malloc'ами каждые 30мс!
    // Максимальный пакет 32 * 12 = 384 байта
    static uint8_t buf[384]; 
    
    if (bytes_to_read > sizeof(buf)) {
        bytes_to_read = sizeof(buf);
        num_packets = sizeof(buf) / 12;
    }

    esp_err_t err = mpu_read_reg(MPU6050_REG_FIFO_R_W, buf, bytes_to_read);
    if (err == ESP_OK) {
        for (int i = 0; i < num_packets; i++) {
            int idx = i * 12;
            data_out[i].accel_x = (int16_t)((buf[idx] << 8) | buf[idx + 1]);
            data_out[i].accel_y = (int16_t)((buf[idx + 2] << 8) | buf[idx + 3]);
            data_out[i].accel_z = (int16_t)((buf[idx + 4] << 8) | buf[idx + 5]);
            data_out[i].gyro_x  = (int16_t)((buf[idx + 6] << 8) | buf[idx + 7]);
            data_out[i].gyro_y  = (int16_t)((buf[idx + 8] << 8) | buf[idx + 9]);
            data_out[i].gyro_z  = (int16_t)((buf[idx + 10] << 8) | buf[idx + 11]);
        }
    }
    return err;
}