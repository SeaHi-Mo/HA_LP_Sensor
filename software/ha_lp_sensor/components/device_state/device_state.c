/**
 * @file device_state.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-07-29
 *
 * @copyright Copyright (c) 2024
 *
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device_state.h"

#define DBG_TAG "DEVICE_STATE"

static QueueHandle_t device_queue_handle;
static TaskHandle_t state_machine_handle;
static homeAssisatnt_device_t ha_dev;

static void device_state_machine(void* arg)
{
    dev_msg_t* dev_msg = pvPortMalloc(sizeof(dev_msg_t));
    dev_msg->ha_dev = &ha_dev;
    while (1)
    {
        xQueueReceive(device_queue_handle, dev_msg, portMAX_DELAY);
        switch (dev_msg->device_state) {
            case DEVICE_STATE_SYSTEM_START:
            {
                HA_LOG_I("DEVICE_STATE_SYSTEM_START\r\n");

                //读取HA 连接信息
                if (flash_get_mqtt_info(&ha_dev.mqtt_info)) {
                    if (flash_get_ha_device_msg(&ha_dev)) {
                        // if (!blufi_is_start)
                        ha_dev.mqtt_info.mqtt_host = "wx.ai-thinker.com";
                        ha_dev.mqtt_info.port = 1883;
                        device_homeAssistant_init(&ha_dev);
                        // flash_save_device_boot_cnt(0);
                    }
                }
            }
            break;
            case DEVICE_STATE_WIFI_START:
            {
                HA_LOG_I("DEVICE_STATE_WIFI_START\r\n");
                //读取WiFi 信息
                int ret = flash_get_wifi_info(&dev_msg->wifi_info);
                HA_LOG_I(" ret=%d\r\n", ret);
                if (strlen(dev_msg->wifi_info.ssid)==0) {
                    memset(dev_msg->wifi_info.ssid, 0, 64);
                    memset(dev_msg->wifi_info.password, 0, 64);
                    sprintf(dev_msg->wifi_info.ssid, "AIOT@FAE");
                    sprintf(dev_msg->wifi_info.password, "fae12345678");
                }
                else {
                    HA_LOG_I("ssid=%s passs=%s  f=%d\r\n", dev_msg->wifi_info.ssid, dev_msg->wifi_info.password, dev_msg->wifi_info.frequency);
                }

                wifi_mgmr_sta_quickconnect(dev_msg->wifi_info.ssid, dev_msg->wifi_info.password, 0, (2412+5*13));
            }
            break;
            case DEVICE_STATE_WIFI_CONNECT:
            {
                HA_LOG_I("DEVICE_STATE_WIFI_CONNECT\r\n");
                flash_save_wifi_info(&dev_msg->wifi_info);
                homeAssistant_device_start();
            }
            break;
            case DEVICE_STATE_WIFI_DISCONNET:
            {
                HA_LOG_I("DEVICE_STATE_WIFI_DISCONNET\r\n");

            }
            break;
            default:
                break;
        }
    }

}

void device_state_machine_start(void)
{
    device_queue_handle = xQueueCreate(2, sizeof(dev_msg_t));

    xTaskCreate(device_state_machine, "state_machine", 1024, NULL, 2, &state_machine_handle);
    static dev_msg_t dev_msg = {
         .device_state = DEVICE_STATE_SYSTEM_START,
    };
    device_state_machine_update(false, &dev_msg);
}

void device_state_machine_stop(void)
{
    vTaskSuspend(state_machine_handle);
    vTaskDelay(state_machine_handle);
}

void device_state_machine_update(int is_iqr, dev_msg_t* dev_msg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (is_iqr) {
        xQueueSendFromISR(device_queue_handle, dev_msg, &xHigherPriorityTaskWoken);
    }
    else {
        xQueueSend(device_queue_handle, dev_msg, portMAX_DELAY);
    }
}