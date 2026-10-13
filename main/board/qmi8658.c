#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "qmi8658.h"

static const char *TAG = "qmi8658";

#define QMI8658_REG_WHO_AM_I 0x00
#define QMI8658_WHO_AM_I_VALUE 0x05
#define QMI8658_REG_CTRL1 0x02
#define QMI8658_REG_CTRL2 0x03
#define QMI8658_REG_CTRL3 0x04
#define QMI8658_REG_CTRL5 0x06
#define QMI8658_REG_CTRL7 0x08
#define QMI8658_REG_AX_L  0x35
#define QMI8658_REG_RESET 0x60
#define QMI8658_RESET_VALUE 0xB0

// CTRL1 bit6: auto-increment the register address on multi-byte reads.
#define QMI8658_CTRL1_AUTOINC 0x40

// CTRL2 = accel range (bits 6:4) | accel output data rate (bits 3:0).
// Range 001 = +-4g, ODR 0101 = 250 Hz normal mode.
#define QMI8658_CTRL2_RANGE4G_ODR250 0x15

// CTRL3 = gyro range | ODR. Only enabled because the sensor samples reliably
// that way on this board; the gyro values themselves are never read.
#define QMI8658_CTRL3_GYRO_512DPS_250HZ 0x54

// CTRL7 bit0 enables the accelerometer, bit1 the gyro.
#define QMI8658_CTRL7_ACCEL_GYRO_ENABLE 0x03

// +-4g over a 16-bit signed reading: 32768 / 4 = 8192 LSB per g.
#define QMI8658_ACCEL_LSB_PER_G 8192.0f

#define QMI8658_XFER_TIMEOUT_MS 100

static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(dev, buf, sizeof(buf), QMI8658_XFER_TIMEOUT_MS);
}

esp_err_t qmi8658_init(i2c_master_dev_handle_t dev)
{
    // WHO_AM_I should read 0x05 on a QMI8658. Checking it up front turns
    // "wrong address / pins / dead sensor" into a clear error instead of an
    // IMU that silently reports a constant 1 g.
    uint8_t reg = QMI8658_REG_WHO_AM_I;
    uint8_t id = 0;
    esp_err_t ret = i2c_master_transmit_receive(dev, &reg, 1, &id, 1, QMI8658_XFER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WHO_AM_I read failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "WHO_AM_I = 0x%02X (expect 0x05)", id);
    if (id != QMI8658_WHO_AM_I_VALUE) {
        return ESP_ERR_NOT_FOUND;
    }

    // Start-up sequence as in the vendor's reference driver. Enabling only
    // the accelerometer left the output registers frozen on this board, so:
    // soft-reset, address auto-increment, accel + gyro both on.
    ESP_RETURN_ON_ERROR(write_reg(dev, QMI8658_REG_RESET, QMI8658_RESET_VALUE), TAG, "reset");
    vTaskDelay(pdMS_TO_TICKS(30));
    ESP_RETURN_ON_ERROR(write_reg(dev, QMI8658_REG_CTRL1, QMI8658_CTRL1_AUTOINC), TAG, "ctrl1");
    ESP_RETURN_ON_ERROR(write_reg(dev, QMI8658_REG_CTRL2, QMI8658_CTRL2_RANGE4G_ODR250), TAG, "ctrl2");
    ESP_RETURN_ON_ERROR(write_reg(dev, QMI8658_REG_CTRL3, QMI8658_CTRL3_GYRO_512DPS_250HZ), TAG, "ctrl3");
    ESP_RETURN_ON_ERROR(write_reg(dev, QMI8658_REG_CTRL5, 0x00), TAG, "ctrl5");
    ESP_RETURN_ON_ERROR(write_reg(dev, QMI8658_REG_CTRL7, QMI8658_CTRL7_ACCEL_GYRO_ENABLE), TAG, "ctrl7");
    // Let the first sample land before anyone reads the output registers.
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

esp_err_t qmi8658_read_accel(i2c_master_dev_handle_t dev, float *x, float *y, float *z)
{
    uint8_t reg = QMI8658_REG_AX_L;
    uint8_t raw[6];
    esp_err_t ret = i2c_master_transmit_receive(dev, &reg, 1, raw, sizeof(raw), QMI8658_XFER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        return ret;
    }

    int16_t ax = (int16_t)((raw[1] << 8) | raw[0]);
    int16_t ay = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t az = (int16_t)((raw[5] << 8) | raw[4]);

    *x = ax / QMI8658_ACCEL_LSB_PER_G;
    *y = ay / QMI8658_ACCEL_LSB_PER_G;
    *z = az / QMI8658_ACCEL_LSB_PER_G;
    return ESP_OK;
}
