/**
 * @file wifi_event.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-06-29
 *
 * @copyright Copyright (c) 2023
 *
*/
#ifndef WIFI_EVENT_H
#define WIFI_EVENT_H

void staWiFiInit(void);

void staWiFiConnect(const char* ssid, const char* passworld);
void staWiFiDisconnect(void);

void blufi_config_start(void);
void wifi_device_stop(void);

void blufi_wifi_event(int event, void* param);
#endif