#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

// Minimal QMI8658 register-level driver: accelerometer only, just enough
// to turn "which way is the keychain tilted" into gesture navigation (see
// main/ui/gesture.c). Not a full IMU/sensor-fusion driver - gyro is left
// disabled since nothing here needs it yet.
//
// board_esp32.c owns the I2C bus/device (config/board_config.h has the
// pins and address); this driver just speaks the QMI8658 register protocol
// over whatever device handle it's given.

esp_err_t qmi8658_init(i2c_master_dev_handle_t dev);

// Reads acceleration in g on each axis (+-4g range as configured by init).
esp_err_t qmi8658_read_accel(i2c_master_dev_handle_t dev, float *x, float *y, float *z);
