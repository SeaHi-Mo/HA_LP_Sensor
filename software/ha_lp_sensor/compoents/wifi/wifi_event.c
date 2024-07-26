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

#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include "bl_fw_api.h"
#include "wifi_mgmr_ext.h"
#include "wifi_mgmr.h"
#include "bflb_irq.h"
#include "bflb_uart.h"
#include "bflb_l1c.h"
#include "bflb_mtimer.h"
#include "bl616_glb.h"
#include "rfparam_adapter.h"
#include "board.h"
#include "log.h"
#include "aiio_wifi.h"


#include "homeAssistantMQTT.h"

#define DBG_TAG "WIFI EVENT"

#define WIFI_STACK_SIZE     (1024*4)
#define TASK_PRIORITY_FW    (16)
static int32_t ret = 0;
static int32_t rssi = 0;
static int32_t state = 0;
static bool wifi_is_connect;
int wifi_config_start = false;
unsigned char mac[MAC_LEN];

aiio_wifi_sta_connect_ind_stat_info_t wifi_ind_stat;

/**
 * @brief cb_wifi_event
 *    连接WiFi的回调
 * @param event
 * @param data
*/
static void cb_wifi_event(aiio_input_event_t* event, void* data)
{
    int32_t ret = 0;
    // uint8_t bssid[6] = {0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5};


    switch (event->code) {
        case AIIO_WIFI_EVENT_WIFI_READY:
            LOG_I("<<<<<<<<<  WIFI INIT OK <<<<<<<<<<");
            // homeassistant_blufi_start();
            staWiFiConnect("FAE@Seahi", "fae12345678");

            // aiio_wifi_scan();
            break;
        case AIIO_WIFI_EVENT_SCAN_DONE:
            LOG_I("<<<<<<<<<  SCAN DONE OK <<<<<<<<<< %d", wifi_mgmr_sta_scanlist());
            // wifi_mgmr_sta_scanlist_dump(data,);

            break;
        case AIIO_WIFI_EVENT_STA_CONNECTING:
            LOG_I("<<<<<<<<< STA_CONNECTING <<<<<<<<<<<");
            break;
        case AIIO_WIFI_EVENT_STA_CONNECTED:
            LOG_I("<<<<<<<<< CONNECTED<<<<<<<<<<<");


            break;
        case AIIO_WIFI_EVENT_STA_DISCONNECTED:
            LOG_I("<<<<<<<<<  DISCONNECTED <<<<<<<<<<");
            if (event->value == AIIO_WLAN_FW_4WAY_HANDSHAKE_ERROR_PSK_TIMEOUT_FAILURE ||
                    event->value == AIIO_WLAN_FW_AUTH_OR_ASSOC_RESPONSE_TIMEOUT_FAILURE) {
                //connect timeout
                LOG_I("connect timeout");
            }
            else if (event->value == AIIO_WLAN_FW_4WAY_HANDSHAKE_TX_DEAUTH_FRAME_TRANSMIT_FAILURE ||
                   event->value == AIIO_WLAN_FW_4WAY_HANDSHAKE_TX_DEAUTH_FRAME_ALLOCATE_FAIILURE ||
                   event->value == AIIO_WLAN_FW_DEAUTH_BY_AP_WHEN_NOT_CONNECTION) {
                //password error
                LOG_I("password error");
            }
            else if (event->value == AIIO_WLAN_FW_SCAN_NO_BSSID_AND_CHANNEL) {
                //not found AP
                LOG_I("not found AP");
            }
            else if ((event->value == AIIO_WLAN_FW_DEAUTH_BY_AP_WHEN_CONNECTION) || (event->value == AIIO_WLAN_FW_DISCONNECT_BY_USER_WITH_DEAUTH)) {
                //wifi disconnect
                LOG_I("wifi disconnect");
            }
            else {
                //connect error
                LOG_I("connect error");
            }
            break;
        case AIIO_WIFI_EVENT_STA_GOT_IP:
        {
            LOG_I("<<<<<<<<< CONNECTED GOT IP <<<<<<<<<<<");
            wifi_is_connect = true;
            aiio_wifi_rssi_get(&rssi);
            LOG_I("wifi cur_rssi = %d!!", rssi);
            aiio_wifi_sta_mac_get(mac);
            LOG_I("wifi mac = %02x%02x%02x%02x%02x%02x!!", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            // aiio_wifi_sta_connect_ind_stat_get(&wifi_ind_stat);
            wifi_mgmr_sta_get_bssid(wifi_ind_stat.bssid);
            LOG_I("wifi sta_bssid = %02x%02x%02x%02x%02x%02x", wifi_ind_stat.bssid[0], wifi_ind_stat.bssid[1], wifi_ind_stat.bssid[2], wifi_ind_stat.bssid[3], wifi_ind_stat.bssid[4], wifi_ind_stat.bssid[5]);

            //下方开始连接服务器
            /************* 发起连接  ********/
            homeAssistant_device_start();
        }
        break;
        default:
            break;
    }
}
/**
 * @brief
 *
*/
void staWiFiInit(void)
{
    // aiio_blufi_set_ble_name("Ai-M61_HA");
    aiio_wifi_register_event_cb(cb_wifi_event);
    int ret = aiio_wifi_init();
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
    if (wifi_is_connect)
        aiio_wifi_disconnect();

    vTaskDelay(pdMS_TO_TICKS(100));
    aiio_wifi_config_t wifi_sta_config;
    memset(wifi_sta_config.sta.ssid, 0, sizeof(wifi_sta_config.sta.ssid));
    memset(wifi_sta_config.sta.password, 0, sizeof(wifi_sta_config.sta.password));

    wifi_sta_config.sta.channel = 0;
    wifi_sta_config.sta.use_dhcp = 1;
    wifi_sta_config.sta.flags = 0;
    wifi_sta_config.sta.band = 0;

    memcpy((char*)wifi_sta_config.sta.ssid, ssid, strlen(ssid));
    memcpy((char*)wifi_sta_config.sta.password, passworld, strlen(passworld));
    LOG_I("ssid = \"%s\" password=\"%s\"", wifi_sta_config.sta.ssid, wifi_sta_config.sta.password);
    aiio_wifi_set_mode(AIIO_WIFI_MODE_STA);
    aiio_wifi_set_config(AIIO_WIFI_IF_STA, &wifi_sta_config);
    aiio_wifi_start();

    wifi_is_connect = false;
}
/**
 * @brief
 *
*/
void staWiFiDisconnect(void)
{
    aiio_wifi_disconnect();

    aiio_os_tick_dealy(aiio_os_ms2tick(2000));

}
