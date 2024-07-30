/**
 * @file easy_flash.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-05-26
 *
 * @copyright Copyright (c) 2024
 *
*/
#ifndef EASY_FLASH_H
#define EASY_FLASH_H

typedef enum {
    FLASH_WIFI_SSID,
    FLASH_WIFI_PASSWORD,
    FLASH_WIFI_PMK,
    FLASH_WIFI_BAND,
    FLASH_WIFI_CHAN_ID,
    FLASH_MQTT_HOST,
    FLASH_MQTT_PORT,
    FLASH_MQTT_CLIENT_ID,
    FLASH_MQTT_USERNAME,
    FLASH_MQTT_PASSWORD,
    FLASH_HA_DEV_NAME,
    FLASH_HA_MANUFACTURER,
    FLASH_DEVICE_BOOT_NUMBLE,
}flash_key_t;

bool flash_save_wifi_info(void* value);
int flash_get_wifi_info(void* value);
bool flash_save_mqtt_info(void* value);
int flash_get_mqtt_info(void* value);
bool flash_save_ha_device_msg(void* value);
int flash_get_ha_device_msg(void* value);
bool ef_del_key(const char* key);
bool flash_save_device_boot_cnt(int numble);
int flash_get_device_boot_cnt(void);
#endif