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
    device_homeAssistant_init(&ha_dev);
    xTaskCreate(device_state_machine, "state_machine", 1024, NULL, 2, &state_machine_handle);
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