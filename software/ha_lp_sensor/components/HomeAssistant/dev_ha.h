/**
 * @file dev_ha.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-06-15
 *
 * @copyright Copyright (c) 2024
 *
*/
#ifndef DEV_HA_H
#define DEV_HA_H

#include "device_state.h"
#include "homeAssistantPort.h"

#define MQTT_SERVER_DEFAULT_HOST "0.0.0.0"
#define MQTT_SERVER_DEFAULT_PORT 1883

#define HA_DEVICE_NAME "传感器采集主板"
#define HA_DEVICE_MANUFACTURER "SeaHi"
#define HA_DEVICE_MODEL "Ai-M62-CBS"

void device_homeAssistant_init(homeAssisatnt_device_t* dev_ha);

#endif