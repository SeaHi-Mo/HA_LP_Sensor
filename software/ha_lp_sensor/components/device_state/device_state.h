/**
 * @file device_state.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-07-29
 *
 * @copyright Copyright (c) 2024
 *
*/
#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H
#include "FreeRTOS.h"
#include "task.h"
#include "dev_ha.h"
#include "log.h"
#include <queue.h>

#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include "export/bl_fw_api.h"
#include "wifi_mgmr_ext.h"
#include "wifi_mgmr.h"

#include "wifi_event.h"
#include "easy_flash.h"
// #include <hal_wifi.h>
#include "axk_ble.h"
#include "blufi.h"
#include "blufi_api.h"
#include "blufi_hal.h"
#include "blufi_init.h"
#include "axk_blufi.h"
#include "ble_interface.h"
#include "blufi_security.h"
#include "bl616_pm.h"
#define DEVICE_HW_SERSION "v1.0.0"

typedef enum {
    DEVICE_STATE_NONE = -1,
    DEVICE_STATE_SYSTEM_START,
    /*WiFi 状态 */
    DEVICE_STATE_WIFI_START,
    DEVICE_STATE_WIFI_SCAN_DONE,
    DEVICE_STATE_WIFI_CONNECT,
    DEVICE_STATE_WIFI_DISCONNET,
    /* 配网状态 */
    DEVICE_STATE_BLUFI_START,
    DEVICE_STATE_BLUFI_CONNECT,
    DEVICE_STATE_BLUFI_GET_WIFI_INFO,
    DEVICE_STATE_BLUFI_GET_SERVER_INFO,
    DEVICE_STATE_BLUFI_DONE,
    /* 自定义状态 */
    DEVICE_STATE_HOMEASSISTANT_CONNECT,
    DEVICE_STATE_HOMEASSISTANT_DATA,
    DEVICE_STATE_HOMEASSISTANT_UPDATE_VALUE,
}dev_state_t;

typedef struct wifi_code_info {
    char ssid[64];
    char password[64];
    char mac[6];
    char pmk[64];
    uint8_t band;
    uint8_t chan_id;
    char ipv4_addr[16];
    uint32_t addr_ip;
    uint16_t frequency;
}wifi_info_t;

typedef struct device_state_handle {
    dev_state_t device_state;
    wifi_info_t wifi_info;
    homeAssisatnt_device_t* ha_dev;
}dev_msg_t;

/**
 * @brief 启动设备状态机
 *
*/
void device_state_machine_start(void);
/**
 * @brief 停止设备状态机
 *
*/
void device_state_machine_stop(void);
/**
 * @brief 更新
 *
 * @param is_iqr
 * @param dev_msg
*/
void device_state_machine_update(int is_iqr, dev_msg_t* dev_msg);
#endif
