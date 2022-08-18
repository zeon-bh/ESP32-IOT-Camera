// Wi-Fi Module of the IOT-Surveillance camera

#ifndef IS_WIFI_H
#define IS_WIFI_H
#endif
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include "sdkconfig.h"

#define WIFI_SSID CONFIG_SSID
#define WIFI_PASS CONFIG_PASSWORD

esp_err_t Init_WiFi(void);