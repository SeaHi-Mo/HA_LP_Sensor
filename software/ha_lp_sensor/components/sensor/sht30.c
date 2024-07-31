/**
 * @file sht30.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-07-26
 *
 * @copyright Copyright (c) 2024
 *
*/
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "dev_sensor.h"
#include "bflb_i2c.h"
#include "bflb_gpio.h"
#define DBG_TAG "SHT30"

#define SHT30_I2C_SDA GPIO_PIN_10
#define SHT30_I2C_SCL GPIO_PIN_11
#define SHT30_CMD_LEN_BYTE                2         /* SHT30芯片指令长度 */
struct bflb_device_s* gpio;
static struct bflb_device_s* i2c0;
static struct bflb_i2c_msg_s msgs[2];

#define SHT30_ADDR 0X44
/* SHT30支持的指令 */
typedef enum
{
    SHT30_CMD_MEAS_CLOCKSTR_H = 0,
    SHT30_CMD_MEAS_CLOCKSTR_M,
    SHT30_CMD_MEAS_CLOCKSTR_L,
    SHT30_CMD_MEAS_POLLING_H,
    SHT30_CMD_MEAS_POLLING_M,
    SHT30_CMD_MEAS_POLLING_L,
    SHT30_CMD_MEAS_PERI_05_H,
    SHT30_CMD_MEAS_PERI_05_M,
    SHT30_CMD_MEAS_PERI_05_L,
    SHT30_CMD_MEAS_PERI_1_H,
    SHT30_CMD_MEAS_PERI_1_M,
    SHT30_CMD_MEAS_PERI_1_L,
    SHT30_CMD_MEAS_PERI_2_H,
    SHT30_CMD_MEAS_PERI_2_M,
    SHT30_CMD_MEAS_PERI_2_L,
    SHT30_CMD_MEAS_PERI_4_H,
    SHT30_CMD_MEAS_PERI_4_M,
    SHT30_CMD_MEAS_PERI_4_L,
    SHT30_CMD_MEAS_PERI_10_H,
    SHT30_CMD_MEAS_PERI_10_M,
    SHT30_CMD_MEAS_PERI_10_L,
    SHT30_CMD_FETCH_DATA,                /* 周期模式的读出测量 */
    SHT30_CMD_ART,                       /* ART（加速响应） */
    SHT30_CMD_STOP_PERI_DATA_ACQU,       /* 中断指令或采集数据 */
    SHT30_CMD_SOFT_RESET,                /* 软复位 */
    SHT30_CMD_RESET_THRO_PIN_NRESET,     /* 硬复位 */
    SHT30_CMD_HEATER_ENABLE,             /* 使能加热器 */
    SHT30_CMD_HEATER_DISABLE,            /* 关闭加热器 */
    SHT30_CMD_READ_STATUS,               /* 读取状态 */
    SHT30_CMD_CLEAR_STATUS,              /* 清除状态 */
    SHT30_CMD_LAST,
}SHT30CMD;

uint8_t SHT30CMDSendData[SHT30_CMD_LAST][SHT30_CMD_LEN_BYTE] =
{
    {0x2C, 0x06},
    {0x2C, 0x0D},
    {0x2C, 0x10},
    {0x24, 0x00},
    {0x24, 0x0B},
    {0x24, 0x16},
    {0x20, 0x32},
    {0x20, 0x24},
    {0x20, 0x2F},
    {0x21, 0x30},
    {0x21, 0x26},
    {0x21, 0x2D},
    {0x22, 0x36},
    {0x22, 0x20},
    {0x22, 0x2B},
    {0x23, 0x34},
    {0x23, 0x22},
    {0x23, 0x29},
    {0x27, 0x37},
    {0x27, 0x21},
    {0x27, 0x2A},
    {0xE0, 0x00},   /* 周期模式的读出测量 */
    {0x2B, 0x32},   /* ART（加速响应） */
    {0x30, 0x93},   /* 中断指令或采集数据 */
    {0x30, 0xA2},   /* 软复位 */
    {0x00, 0x06},   /* 硬复位 */
    {0x30, 0x6D},   /* 使能加热器 */
    {0x30, 0x66},   /* 关闭加热器 */
    {0xF3, 0x2D},   /* 读取状态 */
    {0x30, 0x41},   /* 清除状态 */
};



static void __sht30_i2c_gpio_init(void)
{


    gpio = bflb_device_get_by_name("gpio");
    /* I2C0_SDA */
    bflb_gpio_init(gpio, SHT30_I2C_SDA, GPIO_FUNC_I2C0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    /* I2C0_SCL */
    bflb_gpio_init(gpio, SHT30_I2C_SCL, GPIO_FUNC_I2C0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    //传感器电源使能脚
    bflb_gpio_init(gpio, SENSOR_POWER_EN, GPIO_OUTPUT | GPIO_INPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_0);

}

void sth30_i2c_device_init(void)
{
    __sht30_i2c_gpio_init();
    bflb_gpio_reset(gpio, SENSOR_POWER_EN);

    i2c0 = bflb_device_get_by_name("i2c0");
    bflb_i2c_init(i2c0, 400000);
}

static void sh30_i2c_send_cmd(uint8_t _cmd, uint8_t* sendbuf, uint16_t _length)
{

    msgs[0].addr = SHT30_ADDR;

    msgs[0].flags = I2C_M_NOSTOP;
    msgs[0].buffer = &_cmd;
    msgs[0].length = 1;

    if (_length != 0)
    {
        /* 有数据的发送 */
        msgs[1].addr = SHT30_ADDR;
        msgs[1].flags = 0;
        msgs[1].buffer = sendbuf;
        msgs[1].length = _length;
        bflb_i2c_transfer(i2c0, msgs, 2);
    }
    else
    {
        /* 只发一个字节的数据 */
        msgs[0].flags = 0;
        bflb_i2c_transfer(i2c0, msgs, 1);
    }
}

static void i2c_device_read_data(uint8_t* _Recbuf, uint16_t _length)
{

    msgs[0].addr = SHT30_ADDR;
    msgs[0].flags = I2C_M_READ;
    msgs[0].buffer = _Recbuf;
    msgs[0].length = _length;
    bflb_i2c_transfer(i2c0, msgs, 1);
}

static i2c_sensor_state_t sht30_set_detection_mode(SHT30CMD _Cmd)
{
    if (_Cmd >= SHT30_CMD_LAST)
    {
        return I2C_CMD_SEND_RESU_ERR;
    }
    sh30_i2c_send_cmd(SHT30CMDSendData[_Cmd][0], &SHT30CMDSendData[_Cmd][1], SHT30_CMD_LEN_BYTE - 1);

    //printf("Chip Test Mode CMD send over\r\n");
    return I2C_CMD_SEND_RESU_OK;
}

i2c_sensor_state_t sht30_read_Temperature_and_humidity(float* Temperature, uint8_t* humidity)
{
    uint8_t RecData[7] = { 0 };
    uint16_t Temp = 0;

    if (I2C_CMD_SEND_RESU_ERR == sht30_set_detection_mode(SHT30_CMD_MEAS_CLOCKSTR_H))
    {
        return I2C_READ_ERR;
    }

    /* 等待测量完成 */
    bflb_mtimer_delay_ms(50);
    /* 接收测量数据 */
    i2c_device_read_data(RecData, 6);

    Temp = (RecData[0] << 8) | RecData[1];

    *Temperature = (float)Temp * 175.0 / 65535.0 -45.0;

    Temp = (RecData[3] << 8) | RecData[4];
    *humidity = (Temp * 100) / 65535;

    return I2C_READ_OK;
}
