/**
 * @file batty_adc.c
 * @author your name (you@domain.com)
 * @brief 电池电量ADC 检测程序
 * @version 0.1
 * @date 2024-07-30
 *
 * @copyright Copyright (c) 2024
 *
*/
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "dev_sensor.h"
#include "bflb_adc.h"
#include "bflb_gpio.h"

#define BATTY_ADC_CHANNLE_PIN GPIO_PIN_3
#define DBG_TAG "BATTY ADC"

static struct bflb_device_s* gpio;
static struct bflb_device_s* adc;
static volatile uint32_t raw_data;
static bool adc_irq_flg = false;

static void _batty_adc_gpio_init(void)
{
    gpio = bflb_device_get_by_name("gpio");
    //传感器电源使能脚
    bflb_gpio_init(gpio, SENSOR_ADC_POWER_EN, GPIO_OUTPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_0);
    bflb_gpio_init(gpio, BATTY_ADC_CHANNLE_PIN, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    bflb_gpio_set(gpio, SENSOR_ADC_POWER_EN);
}

static void adc_isr(int irq, void* arg)
{
    uint32_t intstatus = bflb_adc_get_intstatus(adc);
    if (intstatus & ADC_INTSTS_ADC_READY) {
        bflb_adc_int_clear(adc, ADC_INTCLR_ADC_READY);
        raw_data = bflb_adc_read_raw(adc);
        adc_irq_flg = true;
    }
}

void batty_adc_device_init(void)
{
    _batty_adc_gpio_init();
    adc = bflb_device_get_by_name("adc");
    struct bflb_adc_config_s cfg;
    cfg.clk_div = ADC_CLK_DIV_32;
    cfg.scan_conv_mode = false;
    cfg.continuous_conv_mode = false;
    cfg.differential_mode = false;
    cfg.resolution = ADC_RESOLUTION_16B;
    cfg.vref = ADC_VREF_2P0V;
    //adc结构体配置
    struct bflb_adc_channel_s chan;

    chan.pos_chan = ADC_CHANNEL_3;
    chan.neg_chan = ADC_CHANNEL_GND;
    //通道配置，单端模式下neg选择GND，pos注意对应IO口的通道
    bflb_adc_init(adc, &cfg);
    bflb_adc_channel_config(adc, &chan, 1);
    bflb_adc_rxint_mask(adc, false);
    bflb_irq_attach(adc->irq_num, adc_isr, NULL);
    bflb_irq_enable(adc->irq_num);
}
void batty_adc_device_deinit(void)
{
    bflb_gpio_reset(gpio, SENSOR_ADC_POWER_EN);
    bflb_irq_disable(adc->irq_num);
    bflb_adc_deinit(adc);
}

uint32_t batty_get_residual(void)
{


    // bflb_adc_start_conversion(adc);
    // bflb_adc_parse_result(adc, (uint32_t*)&raw_data, &result, 3);

    struct bflb_adc_result_s result;
    bflb_adc_start_conversion(adc);
    bflb_adc_parse_result(adc, (uint32_t*)&raw_data, &result, 1);
    // HA_LOG_D("\r\npos chan %d\r\nADC Value = %d\r\nCurrent Voltage = %d mv\r\n", result.pos_chan, result.value, result.millivolt);
    bflb_adc_stop_conversion(adc);
    bflb_mtimer_delay_ms(20);

    //计算剩余，限制最低电压满值4.2，3.4V

    return result.millivolt;
}