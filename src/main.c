#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <is_wifi.h>
#include <esp_http_client.h>
#include <status_led.h>

static const char* http_link = "http://worldclockapi.com/api/json/utc/now";

// HTTP Client User Event handler function
esp_err_t http_event_handler(esp_http_client_event_t* http_evt){
    switch (http_evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("HTTP DATA = \n%.*s\n",http_evt->data_len,(char*)http_evt->data);
        break;
    
    default:
        return ESP_FAIL;
        break;
    }
    return ESP_OK;
}

void get_http_rest(){
    esp_http_client_config_t client_config = {
        .url = http_link,
        .event_handler = http_event_handler
    };

    esp_http_client_handle_t client_handle = esp_http_client_init(&client_config);
    esp_http_client_perform(client_handle);
    esp_http_client_cleanup(client_handle);
}

void app_main() {
    Init_IS_LED();
    IS_LED_Set_Mode(IS_Setup);
    Init_WiFi();
    vTaskDelay(pdMS_TO_TICKS(2500)); // Wait 2.5 seconds for wifi driver to configure and connect to AP
    IS_LED_Set_Mode(IS_Scan);
    
    while(1){
        get_http_rest();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}