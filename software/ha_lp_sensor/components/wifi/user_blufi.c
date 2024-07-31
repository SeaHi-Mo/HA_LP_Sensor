/**
 * @file user_blufi.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-07-30
 *
 * @copyright Copyright (c) 2024
 *
*/
#if 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <log.h>
#include "cJSON.h"
#include "wifi_event.h"
#include "device_state.h"

#define BLUFI_NAME "M62-Sensor"

#define DBG_TAG "USER_BLUFI"

blufi_config_t g_blufi_config = { 0 };
static dev_msg_t dev_msg = { 0 };
static int scan_counter;
bool ble_is_connected = false;
bool blufi_is_start = false;
static bool gl_sta_connected = false;
static char data_buff[128] = { 0 };
/**
 * @brief Get the ip addr from custom data object
 *     custom_data= {"addr":"xxxxxxxxx","port":1883}
 * @param custom_data
 * @return char*
*/
char* get_ip_addr_from_custom_data(char* custom_data)
{
    if (custom_data==NULL) {
        HA_LOG_E("custom_data is NULL");
        return NULL;
    }
    char* cjson_root = custom_data;
    cJSON* root = cJSON_Parse(cjson_root);

    if (root==NULL)
    {
        HA_LOG_E("%s is't json data", cjson_root);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON* add_type = cJSON_GetObjectItem(root, "mqtt");
    if (add_type==NULL)
    {
        HA_LOG_E("%s not \"mqtt\" project ", cjson_root);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON* addr = cJSON_GetObjectItem(add_type, "addr");
    if (addr==NULL)
    {
        HA_LOG_E("%s not \"addr\" project ", cjson_root);
        cJSON_Delete(root);
        return NULL;
    }
    memset(data_buff, 0, 128);
    strcpy(data_buff, addr->valuestring);
    cJSON_Delete(root);

    return data_buff;
}

uint16_t get_port_from_custom_data(char* custom_data)
{
    if (custom_data==NULL) {
        HA_LOG_E("custom_data is NULL\r\n\033[0m");
        return NULL;
    }
    char* cjson_root = custom_data;
    cJSON* root = cJSON_Parse(cjson_root);
    if (root==NULL)
    {
        HA_LOG_E("%s is't json data\r\n\033[0m", cjson_root);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON* add_type = cJSON_GetObjectItem(root, "mqtt");
    if (add_type==NULL)
    {
        HA_LOG_E("%s not \"mqtt\" project \r\n\033[0m", cjson_root);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON* port_p = cJSON_GetObjectItem(add_type, "port");
    if (port_p==NULL)
    {
        HA_LOG_E("%s not \"port\" project \r\n\033[0m", cjson_root);
        cJSON_Delete(root);
        return NULL;
    }
    uint16_t port = atoi(port_p->valuestring);
    cJSON_Delete(root);
    return port;
}

static void blufi_wifi_event(int event, void* param)
{
    switch (event)
    {

        case BLUFI_STATION_CONNECTED:
            gl_sta_connected = true;
            // wifi_device_stop();

            break;
        case BLUFI_STATION_DISCONNECTED:
            gl_sta_connected = false;

            break;
        case BLUFI_STATION_GOT_IP:
        {
            axk_blufi_extra_info_t info;
            memset(&info, 0, sizeof(axk_blufi_extra_info_t));
            wifi_conn_ap_info_get(&g_blufi_config.wifi.sta);
            memcpy(info.sta_bssid, g_blufi_config.wifi.sta.cwjap_param.bssid, 6);
            info.sta_bssid_set = true;
            info.sta_ssid = (uint8_t*)g_blufi_config.wifi.sta.cwjap_param.ssid;
            info.sta_ssid_len = strlen(g_blufi_config.wifi.sta.cwjap_param.ssid);
            HA_LOG_D("axk_blufi_send_wifi_conn_report\r\n\033[0m");
            if (ble_is_connected == true)
            {
                axk_blufi_send_wifi_conn_report(g_blufi_config.wifi.cwmode, _BLUFI_STA_CONN_SUCCESS, 0, &info);
            }
            else
            {
                printf("BLUFI BLE is not connected yet\r\n\033[0m");
            }

            // printf("BLUFI save ssid&&pwd \r\n");
            g_blufi_config.wifi.cwmode = WIFIMODE_STA;
            blufi_is_start = false;
        }
        break;
        default:
            break;
    }
}

static void example_event_callback(_blufi_cb_event_t event, _blufi_cb_param_t* param)
{
    /* actually, should post to blufi_task handle the procedure,
     * now, as a example, we do it more simply */
    switch (event)
    {
        case AXK_BLUFI_EVENT_INIT_FINISH:
        {
            bt_addr_le_t bt_addr;
            bt_get_local_public_address(&bt_addr);
            memset(g_blufi_config.ble.blufi.blufiname, 0, 31);
            sprintf(g_blufi_config.ble.blufi.blufiname, "%s-%02X%02X", BLUFI_NAME, bt_addr.a.val[1], bt_addr.a.val[0]);
            HA_LOG_I("BLUFI init finish,ble name=%s\r\n\033[0m", g_blufi_config.ble.blufi.blufiname);
            axk_blufi_adv_start();
            blufi_is_start = true;
        }
        break;
        case AXK_BLUFI_EVENT_DEINIT_FINISH:
            HA_LOG_D("BLUFI deinit finish\r\n\033[0m");
            break;
        case AXK_BLUFI_EVENT_BLE_CONNECT:
            HA_LOG_I("BLUFI ble connect\r\n\033[0m");
            ble_is_connected = true;
            axk_blufi_adv_stop();
            blufi_security_init();
            break;
        case AXK_BLUFI_EVENT_BLE_DISCONNECT:

            HA_LOG_W("BLUFI ble disconnect\r\n\033[0m");

            ble_is_connected = false;
            blufi_security_deinit();
            axk_blufi_profile_deinit();
            axk_hal_blufi_deinit();
            axk_blufi_adv_stop();
            axk_hal_ble_role_set(BLE_ROLE_DEINIT);
            break;
        case AXK_BLUFI_EVENT_SET_WIFI_OPMODE:
            HA_LOG_I("BLUFI Set WIFI opmode %d\r\n\033[0m", param->wifi_mode.op_mode);
            // if (axk_hal_wifi_mode_set(WIFIMODE_STA, 0) != BLUFI_ERR_SUCCESS)
            // {
            //    printf("BLUFI axk_hal_wifi_mode_set fail\r");
            //     break;
            // }
            g_blufi_config.wifi.cwmode = WIFIMODE_STA;
            break;
        case AXK_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        {
            cwjap_param_t cwjap_param = { 0 };
            //printf("BLUFI requset wifi connect to AP");
            cwjap_param = g_blufi_config.wifi.sta.cwjap_param;
            if (axk_hal_conn_ap_info_set(&cwjap_param) != BLUFI_ERR_SUCCESS)
            {
                HA_LOG_E("BLUFI axk_hal_conn_ap_info_set fail\r\n\033[0m");
                break;
            }
            g_blufi_config.wifi.sta.state = BLUFI_WIFI_STATE_CONNECTING;
        }

        break;
        case AXK_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
            //printf("BLUFI requset wifi disconnect from AP");
            axk_hal_disconn_ap();
            break;
        case AXK_BLUFI_EVENT_REPORT_ERROR:
            HA_LOG_E("BLUFI report error, error code %d\r\n\033[0m", param->report_error.state);
            axk_blufi_send_error_info(param->report_error.state);
            break;
        case AXK_BLUFI_EVENT_GET_WIFI_STATUS:
        {
            wifi_mode_t mode;
            mode = g_blufi_config.wifi.cwmode;

            if (gl_sta_connected)
            {
                axk_blufi_extra_info_t info;
                memset(&info, 0, sizeof(axk_blufi_extra_info_t));
                wifi_conn_ap_info_get(&g_blufi_config.wifi.sta);
                memcpy(info.sta_bssid, g_blufi_config.wifi.sta.cwjap_param.bssid, 6);
                info.sta_bssid_set = true;
                info.sta_ssid = (uint8_t*)g_blufi_config.wifi.sta.cwjap_param.ssid;
                info.sta_ssid_len = strlen(g_blufi_config.wifi.sta.cwjap_param.ssid);
                axk_blufi_send_wifi_conn_report(mode, _BLUFI_STA_CONN_SUCCESS, 0, &info);
            }
            else
            {
                axk_blufi_send_wifi_conn_report(mode, _BLUFI_STA_CONN_FAIL, 0, NULL);
            }
            HA_LOG_I("BLUFI get wifi status from AP\r\n\033[0m");

            break;
        }
        case AXK_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
            HA_LOG_I("blufi close a gatt connection\r\n\033[0m");
            axk_blufi_disconnect();
            break;
        case AXK_BLUFI_EVENT_DEAUTHENTICATE_STA:
            /* TODO */
            break;
        case AXK_BLUFI_EVENT_RECV_STA_BSSID:
            memset(g_blufi_config.wifi.sta.cwjap_param.bssid, 0, 6);
            memcpy(g_blufi_config.wifi.sta.cwjap_param.bssid, param->sta_bssid.bssid, 6);
            HA_LOG_I("Recv STA BSSID %s\033[0m\r\n", param->sta_bssid.bssid);
            break;
        case AXK_BLUFI_EVENT_RECV_STA_SSID:
            memset(g_blufi_config.wifi.sta.cwjap_param.ssid, 0, 33);
            strncpy(g_blufi_config.wifi.sta.cwjap_param.ssid, (char*)param->sta_ssid.ssid, param->sta_ssid.ssid_len);

            HA_LOG_I("Recv STA SSID %s \033[0m\r\n", (char*)g_blufi_config.wifi.sta.cwjap_param.ssid);
            strncpy(dev_msg.wifi_info.ssid, g_blufi_config.wifi.sta.cwjap_param.ssid, strlen(g_blufi_config.wifi.sta.cwjap_param.ssid));
            break;
        case AXK_BLUFI_EVENT_RECV_STA_PASSWD:
            memset(g_blufi_config.wifi.sta.cwjap_param.pwd, 0, 64);
            strncpy(g_blufi_config.wifi.sta.cwjap_param.pwd, (char*)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
            HA_LOG_I("Recv STA PASSWORD %s\033[0m\r\n", (char*)g_blufi_config.wifi.sta.cwjap_param.pwd);
            strncpy(dev_msg.wifi_info.password, g_blufi_config.wifi.sta.cwjap_param.pwd, strlen(g_blufi_config.wifi.sta.cwjap_param.pwd));
            break;
        case AXK_BLUFI_EVENT_RECV_SOFTAP_SSID:
            break;
        case AXK_BLUFI_EVENT_RECV_SOFTAP_PASSWD:
            break;
        case AXK_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM:
            break;
        case AXK_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE:
            break;
        case AXK_BLUFI_EVENT_RECV_SOFTAP_CHANNEL:
            break;
        case AXK_BLUFI_EVENT_GET_WIFI_LIST:
            // wifi_scan_start();
            break;
        case AXK_BLUFI_EVENT_RECV_CUSTOM_DATA:
            HA_LOG_I("Recv Custom Data len:%d\r\n\033[0m", param->custom_data.data_len);
            HA_LOG_I("Custom Data:%.*s\r\n", param->custom_data.data_len, param->custom_data.data);
            // echo
            axk_blufi_send_custom_data(param->custom_data.data, param->custom_data.data_len);
            {
                ha_mqtt_info_t mqtt_info;
                mqtt_info.mqtt_host = get_ip_addr_from_custom_data((char*)param->custom_data.data);
                mqtt_info.port = get_port_from_custom_data((char*)param->custom_data.data);
                flash_save_mqtt_info(&mqtt_info);
                HA_LOG_I("addr=%s port=%d\r\n\033[0m", mqtt_info.mqtt_host, mqtt_info.port);
            }

            break;
        case AXK_BLUFI_EVENT_RECV_USERNAME:
            /* Not handle currently */
            break;
        case AXK_BLUFI_EVENT_RECV_CA_CERT:
            /* Not handle currently */
            break;
        case AXK_BLUFI_EVENT_RECV_CLIENT_CERT:
            /* Not handle currently */
            break;
        case AXK_BLUFI_EVENT_RECV_SERVER_CERT:
            /* Not handle currently */
            break;
        case AXK_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY:
            /* Not handle currently */
            break;
        case AXK_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
            /* Not handle currently */
            break;
        default:
            break;
    }
}

static _blufi_callbacks_t _callbacks = {
    .event_cb = example_event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func = blufi_aes_encrypt,
    .decrypt_func = blufi_aes_decrypt,
    .checksum_func = blufi_crc_checksum,
};

void blufi_config_start(void)
{
    int ret = -1;
    wifi_interface_init(blufi_wifi_event);
    axk_hal_blufi_init();

    ret = _blufi_host_and_cb_init(&_callbacks);
    if (ret)
    {
        HA_LOG_E("%s initialise failed: %d\r\n\033[0m", __func__, ret);
    }

}
#endif
