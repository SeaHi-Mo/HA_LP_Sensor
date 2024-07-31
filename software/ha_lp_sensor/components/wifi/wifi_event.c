/**
 * @file wifi_event.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-06-29
 *
 * @copyright Copyright (c) 2023
 *
*/
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "string.h"

#include "bflb_irq.h"
#include "bflb_uart.h"
#include "bflb_l1c.h"
#include "bflb_mtimer.h"
#include "bl616_glb.h"
#include "rfparam_adapter.h"
#include "board.h"
#include "log.h"
#include "device_state.h"

#define DBG_TAG "WIFI EVENT"

#define WIFI_STACK_SIZE     (1024*4)
#define TASK_PRIORITY_FW    (16)

static int32_t ret = 0;
static int32_t rssi = 0;
static int32_t state = 0;
static bool wifi_is_connect;
int wifi_config_start = false;
static dev_msg_t dev_msg;
static TaskHandle_t wifi_fw_task;



static wifi_conf_t conf = {
    .country_code = "CN",
};

void wifi_event_handler(uint32_t code)
{
    switch (code) {
        case CODE_WIFI_ON_INIT_DONE: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_INIT_DONE\r\n", __func__);
            wifi_mgmr_init(&conf);
        } break;
        case CODE_WIFI_ON_MGMR_DONE: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_MGMR_DONE\r\n", __func__);
            dev_msg.device_state = DEVICE_STATE_WIFI_START;
            device_state_machine_update(true, &dev_msg);
        } break;
        case CODE_WIFI_ON_SCAN_DONE: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_SCAN_DONE\r\n", __func__);
            wifi_mgmr_sta_scanlist();
        } break;
        case CODE_WIFI_ON_CONNECTED: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_CONNECTED\r\n", __func__);
            if (ble_is_connected) {

                blufi_wifi_evt_export(BLUFI_STATION_CONNECTED);
            }
            // void mm_sec_keydump();
            // mm_sec_keydump();
        } break;
        case CODE_WIFI_ON_GOT_IP: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_GOT_IP\r\n", __func__);

            wifi_mgmr_connect_ind_stat_info_t connect_info;

            dev_msg.device_state = DEVICE_STATE_WIFI_CONNECT;
            wifi_mgmr_sta_connect_ind_stat_get(&connect_info);
            dev_msg.wifi_info.band = connect_info.chan_band;
            dev_msg.wifi_info.chan_id = connect_info.channel;
            dev_msg.wifi_info.frequency = 2412+5*13;
            strcpy(dev_msg.wifi_info.ssid, connect_info.ssid);
            strcpy(dev_msg.wifi_info.password, connect_info.passphr);
            device_state_machine_update(true, &dev_msg);


        } break;
        case CODE_WIFI_ON_DISCONNECT: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_DISCONNECT\r\n", __func__);
            dev_msg.device_state = DEVICE_STATE_WIFI_DISCONNET;
            device_state_machine_update(true, &dev_msg);
        } break;
        case CODE_WIFI_ON_AP_STARTED: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_AP_STARTED\r\n", __func__);
        } break;
        case CODE_WIFI_ON_AP_STOPPED: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_AP_STOPPED\r\n", __func__);
        } break;
        case CODE_WIFI_ON_AP_STA_ADD: {
            LOG_I("[APP] [EVT] [AP] [ADD] %lld\r\n", xTaskGetTickCount());
        } break;
        case CODE_WIFI_ON_AP_STA_DEL: {
            LOG_I("[APP] [EVT] [AP] [DEL] %lld\r\n", xTaskGetTickCount());
        } break;
        default: {
            LOG_I("[APP] [EVT] Unknown code %u \r\n", code);
        }
    }
}
/**
 * @brief
 *
*/
void staWiFiInit(void)
{

    tcpip_init(NULL, NULL);
    LOG_I("Starting wifi ...\r\n");

    /* enable wifi clock */

    GLB_PER_Clock_UnGate(GLB_AHB_CLOCK_IP_WIFI_PHY | GLB_AHB_CLOCK_IP_WIFI_MAC_PHY | GLB_AHB_CLOCK_IP_WIFI_PLATFORM);
    GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_WIFI);

    /* Enable wifi irq */

    extern void interrupt0_handler(void);
    bflb_irq_attach(WIFI_IRQn, (irq_callback)interrupt0_handler, NULL);
    bflb_irq_enable(WIFI_IRQn);

    xTaskCreate(wifi_main, (char*)"fw", WIFI_STACK_SIZE, NULL, TASK_PRIORITY_FW, &wifi_fw_task);

    wifi_is_connect = false;

}
/**
 * @brief staWiFiConnect
 *
 * @param ssid
 * @param password
*/
void staWiFiConnect(const char* ssid, const char* passworld)
{


    wifi_is_connect = false;
}
/**
 * @brief
 *
*/
void staWiFiDisconnect(void)
{


    aiio_os_tick_dealy(aiio_os_ms2tick(2000));

}
