#include <is_wifi.h>

static const char* TAG = "IS_WiFi";

static void wifi_event_handler();
static esp_err_t Init_NVS(void);


// WiFi system event handler function
static void wifi_event_handler(void* wifi_handler_arg, esp_event_base_t wifi_event_base,
                                 int32_t wifi_event_id, void* wifi_event_data){
    switch (wifi_event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG,"Connecting to AP = %s....",WIFI_SSID);
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG,"Successfully connected to %s",WIFI_SSID);
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG,"Disconnected from %s",WIFI_SSID);
        break;
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG,"Static IP Address acquired");
        break;
    
    default:
        ESP_LOGE(TAG,"Event handler Error. EVENT_ID = %d",wifi_event_id);
        break;
    }
}

// Initialize and check the non-volatile flash storage for wifi config data
static esp_err_t Init_NVS(void){
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    return ret;
}

// Initialize the WiFi subsystem stack
void Init_WiFi(){
    esp_err_t ret = Init_NVS();
    if(ret != ESP_OK){
        ESP_LOGE(TAG,"NVS Initialization failed!!!");
        ESP_ERROR_CHECK(ret);
    }

    // Initialize esp network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_def_init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_def_init));

    // Configure WiFi and add event handler
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event_handler,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event_handler,NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // Set the ESP32 as wifi station

    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA,&wifi_configuration));

    // Start and connect the WiFi module
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}