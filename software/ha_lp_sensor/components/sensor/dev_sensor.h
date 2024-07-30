/**
 * @file dev_sensor.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-07-26
 *
 * @copyright Copyright (c) 2024
 *
*/
#ifndef DEV_SENSOR_H
#define DEV_SENSOR_H
#include "device_state.h"
#define SENSOR_POWER_EN 1

typedef enum
{
    I2C_CMD_SEND_RESU_OK = 0,
    I2C_CMD_SEND_RESU_ERR,
    I2C_CMD_SEND_RESU_LAST,
    I2C_READ_OK,
    I2C_READ_ERR,
    I2C_READ_LAST,
}i2c_sensor_state_t;
void sth30_i2c_device_init(void);
i2c_sensor_state_t sht30_read_Temperature_and_humidity(float* Temperature, uint8_t* humidity);
#endif