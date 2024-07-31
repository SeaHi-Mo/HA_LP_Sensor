/**
 * @file dev_ha.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-06-15
 *
 * @copyright Copyright (c) 2024
 *
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device_state.h"

#define DBG_TAG "HomeAssistant"
static dev_msg_t dev_msg;

void ha_event_cb(ha_event_t event, homeAssisatnt_device_t* dev)
{
    switch (event)
    {
        /*  连接服务器事件  */
        case HA_EVENT_MQTT_CONNECED:
            HA_LOG_I("<<<<<<<<<<  HA_EVENT_MQTT_CONNECED\r\n");

            static ha_sensor_entity_t T_sensor = {
                .name = "温度传感器",
                .unique_id = "T01",
                .device_class = Class_temperature,
                .qos = 1,
            };
            static ha_sensor_entity_t H_sensor = {
                .name = "湿度传感器",
                .unique_id = "H01",
                .device_class = Class_humidity,
                .qos = 1,
            };
            static ha_sensor_entity_t BAT_sensor = {
                .name = "电池电量",
                .unique_id = "bat01",
                .device_class = Class_battery,
                .qos = 1,
            };
            homeAssistant_device_add_entity(CONFIG_HA_ENTITY_SENSOR, &T_sensor);
            homeAssistant_device_add_entity(CONFIG_HA_ENTITY_SENSOR, &H_sensor);
            homeAssistant_device_add_entity(CONFIG_HA_ENTITY_SENSOR, &BAT_sensor);
            homeAssistant_device_send_status(HOMEASSISTANT_STATUS_ONLINE);

            dev_msg.device_state = DEVICE_STATE_HOMEASSISTANT_CONNECT;
            // homeAssistant_device_send_entity_state(CONFIG_HA_ENTITY_SENSOR, &BAT_sensor, 100);
            device_state_machine_update(false, &dev_msg);

            break;
            /*  服务器断开事件  */
        case HA_EVENT_MQTT_DISCONNECT:
            HA_LOG_I("<<<<<<<<<<  HA_EVENT_MQTT_DISCONNECT\r\n");
            dev_msg.device_state = DEVICE_STATE_HOMEASSISTANT_DISCONNECT;
            // homeAssistant_device_send_entity_state(CONFIG_HA_ENTITY_SENSOR, &BAT_sensor, 100);
            device_state_machine_update(false, &dev_msg);
            break;
            // case HA_EVENT_MQTT_COMMAND_SWITCH:
            //     // homeAssistant_device_send_entity_state(CONFIG_HA_ENTITY_SWITCH, &sw1, dev->entity_switch->command_switch->switch_state);
            //     // dev_msg.device_state = DEVICE_STATE_HOMEASSISTANT_SWTICC_STATE;
            //     // dev_msg.ha_dev = dev;
            //     // device_state_update(false, &dev_msg);

            //     break;
        default:
            break;
    }
    event = HA_EVENT_NONE;
}

void device_homeAssistant_init(homeAssisatnt_device_t* dev_ha)
{
    if (dev_ha==NULL)
    {
        HA_LOG_E("dev_ha is NULL\r\n");
        return;
    }
    uint8_t MAC[6] = { 0 };

    wifi_mgmr_sta_mac_get(MAC);
    if (dev_ha->name==NULL) dev_ha->name = HA_DEVICE_NAME;
    dev_ha->hw_version = DEVICE_HW_SERSION;
    if (dev_ha->manufacturer==NULL) dev_ha->manufacturer = HA_DEVICE_MANUFACTURER;
    if (dev_ha->model==NULL) dev_ha->model = HA_DEVICE_MODEL;
    if (dev_ha->identifiers==NULL) {
        dev_ha->identifiers = pvPortMalloc(64);
        memset(dev_ha->identifiers, 0, 64);
        sprintf(dev_ha->identifiers, "%s-%02X%02X", dev_ha->name, MAC[4], MAC[5]);
    }

    if (dev_ha->mqtt_info.mqtt_host==NULL) dev_ha->mqtt_info.mqtt_host = MQTT_SERVER_DEFAULT_HOST;
    if (dev_ha->mqtt_info.port==0)dev_ha->mqtt_info.port = MQTT_SERVER_DEFAULT_PORT;

    if (dev_ha->mqtt_info.mqtt_clientID==NULL) {
        dev_ha->mqtt_info.mqtt_clientID = pvPortMalloc(64);
        memset(dev_ha->mqtt_info.mqtt_clientID, 0, 64);
        sprintf(dev_ha->mqtt_info.mqtt_clientID, "%s-%02X%02X%02X%02X%02X%02X", dev_ha->name, MAC[0], MAC[1], MAC[2], MAC[3], MAC[4], MAC[5]);
    }
    if (dev_ha->mqtt_info.mqtt_username==NULL) dev_ha->mqtt_info.mqtt_username = dev_ha->name;
    if (dev_ha->mqtt_info.mqtt_password==NULL)dev_ha->mqtt_info.mqtt_password = "12345678";

    dev_ha->mqtt_info.mqtt_keeplive = 60*60;//一个小时的心跳
    HA_LOG_I("------------------mqtt msg----------------------\r\n");
    HA_LOG_I("mqtt host:%s\r\n", dev_ha->mqtt_info.mqtt_host);
    HA_LOG_I("mqtt port:%d\r\n", dev_ha->mqtt_info.port);
    HA_LOG_I("mqtt client ID:%s\r\n", dev_ha->mqtt_info.mqtt_clientID==NULL?"null":dev_ha->mqtt_info.mqtt_clientID);
    HA_LOG_I("--------HomeAssistant device massege------------\r\n");
    HA_LOG_I("device name:%s\r\n", dev_ha->name);
    HA_LOG_I("device manufacturer:%s\r\n", dev_ha->manufacturer);
    HA_LOG_I("------------------------------------------------\r\n");

    homeAssistant_device_init(dev_ha, ha_event_cb);
}
