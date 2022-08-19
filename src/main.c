#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/portmacro.h>
#include <is_wifi.h>
#include <status_led.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <is_camera.h>
#include <hc_ultrasound.h>
#include <button.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define SERVER_IP CONFIG_IPV4_ADDR
#define SERVER_PORT CONFIG_PORT
#define RETRY_MAX 10
#define SETUP_BUTTON GPIO_NUM_2
#define MAX_CAMERA_BURST 15

// Logging Tag
static const char* TAG = "IS_MAIN";

// Event Groups
static EventGroupHandle_t IS_Event;
const uint8_t SETUP_BIT = (1<<0);
const uint8_t DETECTED_BIT = (1<<1);
const uint8_t COMPLETED_BIT = (1<<2);

// HC Sensor struct
typedef struct {
    RingbufHandle_t hc_bufhandler;
    US_detect_range_t range;
    bool isSet;
} hc_sensor_data_t;

// Tasks
void Task_Scan(void* args);


// Log Fatal Errors to the serial monitor
void Is_Error_report(const char* err_msg){
    IS_LED_Set_Mode(IS_Error);

    while(1){
        ESP_LOGE(TAG,"%s",err_msg);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Setup a socket connection and return the socket
int Init_Socket(){
    char* server_ip = SERVER_IP;
    struct sockaddr_in dest_addr = {
        .sin_addr.s_addr = inet_addr(server_ip),
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
    };

    // Using TCP IPV4
    int sock = socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
    if(sock < 0){
        ESP_LOGE(TAG,"Failed to create socket");
        Is_Error_report("Socket Creation Failed");
    }
    ESP_LOGI(TAG,"Socket created successfully, connect to IP:%s PORT:%d",server_ip,SERVER_PORT);

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        for(uint8_t i = 1; i <= RETRY_MAX; i++){
            ESP_LOGI(TAG,"Retrying... [Count = %d]",i);
            err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in));
            if(err == 0) break; // Connection successfull
            vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 sec before retrying
        }
        if(err !=0){
            Is_Error_report("Socket Connection Failed");
        }
    }

    ESP_LOGI(TAG, "Successfully connected");
    return sock;
}

// Button handler function
void button_handler(button_event_t event,void* context){
    hc_sensor_data_t* hc_sensor = (hc_sensor_data_t*)context;
    switch(event){
        case button_event_single_press:
            // Disable single press if already set
            if(!hc_sensor->isSet){
                float distance = HC_Get_Range(hc_sensor->hc_bufhandler);
                ESP_LOGI(TAG,"Setup Distance = %f",distance);

                // Range Tolerance = 5%
                hc_sensor->range.range_max = distance * 1.05;
                hc_sensor->range.range_min = distance * 0.95;
                hc_sensor->isSet = true;
                xEventGroupSetBits(IS_Event,SETUP_BIT);
            }
            break;
        case button_event_long_press:
            IS_LED_Set_Mode(IS_Setup);
            hc_sensor->isSet = false;
            ESP_LOGI(TAG,"Setup Mode");
            xTaskCreatePinnedToCore(Task_Scan,"HC Scanner",4096,hc_sensor,1,NULL,1);
            break;
        default:
            ESP_LOGE(TAG,"Button handler error!!!!!");
            break;
    }
}

// Start the Setup Process
void Init_Button(hc_sensor_data_t* hc_sensor){
    IS_LED_Set_Mode(IS_Setup);
    
    button_config_t setup_btn = BUTTON_CONFIG(button_active_low,.long_press_time = 1500);
    int err = button_create(SETUP_BUTTON,setup_btn,button_handler,hc_sensor);
    if(err){
        Is_Error_report("Failed to initialize setup button!!!");
    }
}

// Start the surveillance task
void Task_Scan(void* args){
    // Wait for the setup task to set the range data
    xEventGroupWaitBits(IS_Event,SETUP_BIT,pdTRUE,pdFALSE,portMAX_DELAY);
    IS_LED_Set_Mode(IS_Scan);
    hc_sensor_data_t* hc_sensor = (hc_sensor_data_t*)args;
    portMUX_TYPE checkMUTEX = portMUX_INITIALIZER_UNLOCKED;
    float distance;
    bool detected = false;

    while(hc_sensor->isSet){
        distance = HC_Get_Range(hc_sensor->hc_bufhandler);
    
        taskENTER_CRITICAL(&checkMUTEX);
        if(distance > hc_sensor->range.range_max || distance < hc_sensor->range.range_min){
            detected = true;
        }
        taskEXIT_CRITICAL(&checkMUTEX);

        if(detected){
            // signal camera task and block
            xEventGroupSetBits(IS_Event,DETECTED_BIT);
            xEventGroupWaitBits(IS_Event,COMPLETED_BIT,pdTRUE,pdFALSE,portMAX_DELAY);
            detected = false;
        }
        
        // Poll sensor every 100ms [10hz]
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG,"HC Scanner has stopped");
    vTaskDelete(NULL);
}

// Camera task captures bursts of 15 pictures and sends it to the server
void Task_Camera(void* args){
    int sock = *((int*)args);

    while (1) {
        // Wait for detect signal before taking pictures
        xEventGroupWaitBits(IS_Event,DETECTED_BIT,pdTRUE,pdFALSE,portMAX_DELAY);

        for(uint8_t count = 0; count < MAX_CAMERA_BURST; count++){
            camera_fb_t* fb = IS_Camera_Capture();
            size_t* bf_size = &(fb->len);
            
            // Send buffer size
            int err = send(sock, bf_size, sizeof(size_t), 0);
            ESP_LOGI(TAG,"Camera Buffer Size = %d",fb->len);
            // Send the buffer data
            err = send(sock, fb->buf, fb->len, 0);

            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                for(uint8_t i = 1; i <= RETRY_MAX; i++){
                    ESP_LOGI(TAG,"Retrying... [Count = %d]",i);
                    err = send(sock, fb->buf, fb->len, 0);
                    if(err == 0) break; // Data Sent Successfully
                    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 sec before retrying
                }
                if(err !=0){
                    IS_Return_buffer(fb);
                    Is_Error_report("Failed to send data to the server!!!");
                } 
            }
            IS_Return_buffer(fb);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
        ESP_LOGI(TAG,"Finished Camera Task.");
        xEventGroupSetBits(IS_Event,COMPLETED_BIT); // Resume Scanning
    }
}

void app_main() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable Brownout Detector

    esp_err_t err = ESP_OK;
    Init_IS_LED();
    // IS_LED_Set_Mode(IS_Setup); // Default mode on startup

    err = Init_WiFi();
    vTaskDelay(pdMS_TO_TICKS(2500)); // Wait 2.5 seconds for wifi driver to configure and connect to AP
    if (err != ESP_OK) Is_Error_report("WIFI ERROR");

    // Initialize the camera module
    err = IS_Camera_Init();
    if (err != ESP_OK) Is_Error_report("CAMERA ERROR");

    // Initialize the HC Sensor
    err = HC_Trig_Init();
    if (err != ESP_OK) Is_Error_report("HC Sensor - Trigger Init error ERROR");
    RingbufHandle_t hc_buffer = HC_Echo_Init();
    if (hc_buffer == NULL) Is_Error_report("HC Sensor - Echo Init error ERROR");

    // Create Event Group
    IS_Event = xEventGroupCreate();

    // Allocate range data struct
    hc_sensor_data_t* hc_sensor = (hc_sensor_data_t*)malloc(sizeof(hc_sensor_data_t));
    hc_sensor->hc_bufhandler = hc_buffer;
    hc_sensor->isSet = false;

    // Start the socket connection
    int socket = Init_Socket();

    Init_Button(hc_sensor);
    xTaskCreatePinnedToCore(Task_Scan,"HC Scanner",4096,hc_sensor,2,NULL,1);
    xTaskCreatePinnedToCore(Task_Camera,"IS Camera",8192,&socket,3,NULL,1);
}