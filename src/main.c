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
static SemaphoreHandle_t IS_semaphore;

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
        vTaskDelay(pdMS_TO_TICKS(5000));
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
    int sock = -1; int err = 1;
    uint8_t rcount = 1;

    // Using TCP IPV4
    while(sock < 0 || err != 0){
        if(rcount == RETRY_MAX){
            Is_Error_report("Couldnt Connect to Server. Check if server connection is open");
        } else {
            sock = socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
            err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in));

            if(sock < 0 || err != 0){
                if(sock < 0){
                    ESP_LOGE(TAG,"Failed to create socket, Retrying....");
                }
                if (err != 0){
                    ESP_LOGE(TAG, "Socket unable to connect: errno %d, Retrying.....", errno);
                }
            } else break;
        }
        rcount++;
        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 sec before retrying
    }

    ESP_LOGI(TAG,"Socket connection created successfully, connected to IP:%s PORT:%d",server_ip,SERVER_PORT);
    return sock;
}

// Button handler function
void button_handler(button_event_t event,void* context){
    hc_sensor_data_t* hc_sensor = (hc_sensor_data_t*)context;
    xSemaphoreTake(IS_semaphore,portMAX_DELAY);
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
    xSemaphoreGive(IS_semaphore);
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
    float distance;
    bool detected = false;

    while(hc_sensor->isSet){
        xSemaphoreTake(IS_semaphore,portMAX_DELAY);
        distance = HC_Get_Range(hc_sensor->hc_bufhandler);
        if(distance > hc_sensor->range.range_max || distance < hc_sensor->range.range_min){
            detected = true;
        }
        xSemaphoreGive(IS_semaphore);

        if(detected){
            // Signal camera task and block
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
    typedef struct {
        uint8_t* frame_data;
        size_t frame_len;
    }SPRAM_Frame_Data_t;
    SPRAM_Frame_Data_t frame_buffer[MAX_CAMERA_BURST];

    while (1) {
        // Wait for detect signal before taking pictures
        xEventGroupWaitBits(IS_Event,DETECTED_BIT,pdTRUE,pdFALSE,portMAX_DELAY);

        xSemaphoreTake(IS_semaphore,portMAX_DELAY);
        // Capture Camera bursts
        ESP_LOGI(TAG,"Capturing frames....");
        for(uint8_t count = 0; count < MAX_CAMERA_BURST; count++){
            camera_fb_t* fb = IS_Camera_Capture();
            frame_buffer[count].frame_data = (uint8_t*)malloc(fb->len);
            frame_buffer[count].frame_len = fb->len;
            // Copy frame data from DMA buffer to external RAM
            memcpy(frame_buffer[count].frame_data,fb->buf,fb->len);
            IS_Return_buffer(fb);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        xSemaphoreGive(IS_semaphore);

        xSemaphoreTake(IS_semaphore,portMAX_DELAY);
        // Send the camera_fb buffer to server
        ESP_LOGI(TAG,"Sending captured frames to server....");
        for(uint8_t count = 0; count < MAX_CAMERA_BURST;){
            int err = send(sock, &frame_buffer[count].frame_len, sizeof(size_t), 0);
            ESP_LOGI(TAG,"Camera Buffer Size = %d",frame_buffer[count].frame_len);
            // Send the buffer data
            err = send(sock, frame_buffer[count].frame_data, frame_buffer[count].frame_len, 0);

            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d, Restarting socket connection....", errno);
                // Re-initialize socket connection to server before sending image data
                Init_Socket();
            } else{
                count++;
            }
            vTaskDelay(pdMS_TO_TICKS(500)); // Try 100ms
        }
        xSemaphoreGive(IS_semaphore);

        // Free the allocated space in external SPIRAM
        for(uint8_t frame = 0;frame < MAX_CAMERA_BURST;frame++){
            free(frame_buffer[frame].frame_data);
            frame_buffer[frame].frame_len = 0;
        }

        ESP_LOGI(TAG,"Finished Camera Task.");
        closesocket(sock);
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

    // Create Event Group and Binary Semaphore
    IS_Event = xEventGroupCreate();
    IS_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(IS_semaphore);

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