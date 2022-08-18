#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <is_wifi.h>
#include <status_led.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <is_camera.h>
#include <hc_ultrasound.h>
#include <button.h>

#define SERVER_IP CONFIG_IPV4_ADDR
#define SERVER_PORT CONFIG_PORT
#define RETRY_MAX 10
#define SETUP_BUTTON GPIO_NUM_2

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
} hc_sensor_data_t;

// Log Fatal Errors to the serial monitor
void Is_Error_report(const char* err_msg,esp_err_t err,TaskHandle_t err_Task){
    IS_LED_Set_Mode(IS_Error);
    if (err_Task != NULL){
        vTaskDelete(err_Task);
    }

    while(1){
        ESP_LOGE(TAG,"%s",err_msg);
        ESP_ERROR_CHECK(err);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Button handler function
void button_handler(button_event_t event,void* context){
    hc_sensor_data_t* hc_sensor = (hc_sensor_data_t*)context;
    float distance = HC_Get_Range(hc_sensor->hc_bufhandler);

    // Range Tolerance = 5%
    hc_sensor->range.range_max = distance * 1.05;
    hc_sensor->range.range_min = distance * 0.95;
    
    xEventGroupSetBits(IS_Event,SETUP_BIT);
}

void task_socket(void* args){
    char* server_ip = SERVER_IP;

    while(1){
        struct sockaddr_in dest_addr = {
        .sin_addr.s_addr = inet_addr(server_ip),
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        };

        // Using TCP IPV4
        int sock = socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
        if(sock < 0){
            ESP_LOGE(TAG,"Failed to create socket");
            goto end_sock;
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
                ESP_LOGE(TAG,"Socket connection failed!!!");
                goto end_sock;
            }
        }
        ESP_LOGI(TAG, "Successfully connected");


        vTaskDelay(pdMS_TO_TICKS(2000)); // Remove this later

        while (1) {
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
                    vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 sec before retrying
                }
                if(err !=0){
                    IS_Return_buffer(fb);
                    ESP_LOGE(TAG,"Failed to send data to the server!!!");
                    goto end_sock;
                } 
            }
            IS_Return_buffer(fb);
            ESP_LOGI(TAG,"Finished sending Frame.");
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }

end_sock:
    IS_LED_Set_Mode(IS_Error);
    ESP_LOGE(TAG,"FATAL ERROR!!! Socket Task Terminating");
    vTaskDelete(NULL);
}

// Start the surveillance task
void Task_Scan(void* args){
    IS_LED_Set_Mode(IS_Setup);
    hc_sensor_data_t hc_sensor = {
        .hc_bufhandler = (RingbufHandle_t)args,
    };

    button_config_t setup_btn = BUTTON_CONFIG(button_active_low);
    int err = button_create(SETUP_BUTTON,setup_btn,button_handler,&hc_sensor);
    if(err){
        Is_Error_report("Failed to initialize setup button!!!",ESP_FAIL,Task_Scan);
    }

    ESP_LOGI(TAG,"Setup button initialized,waiting for range data.....");
    // Wait for User Input
    xEventGroupWaitBits(IS_Event,SETUP_BIT,pdTRUE,pdTRUE,portMAX_DELAY);
    IS_LED_Set_Mode(IS_Scan);

    // Start the scanning process
    while(1){
        float distance = HC_Get_Range(hc_sensor.hc_bufhandler);
        ESP_LOGI(TAG,"Distance = %f",distance);

        if(distance > hc_sensor.range.range_max || distance < hc_sensor.range.range_min){
            ESP_LOGI(TAG,"INTRUDER DETECTED !!!!");
        }

        // Poll sensor every 100ms [10hz]
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    Is_Error_report("Task = Task_Scan Unknown Error",ESP_FAIL,Task_Scan);
}

void app_main() {
    esp_err_t err = ESP_OK;
    Init_IS_LED();
    // IS_LED_Set_Mode(IS_Setup); // Default mode on startup

    err = Init_WiFi();
    vTaskDelay(pdMS_TO_TICKS(2500)); // Wait 2.5 seconds for wifi driver to configure and connect to AP
    if (err != ESP_OK) Is_Error_report("WIFI ERROR",err,NULL);

    // Initialize the camera module
    err = IS_Camera_Init();
    if (err != ESP_OK) Is_Error_report("CAMERA ERROR",err,NULL);

    // Initialize the HC Sensor
    err = HC_Trig_Init();
    if (err != ESP_OK) Is_Error_report("HC Sensor - Trigger Init error ERROR",err,NULL);
    RingbufHandle_t hc_buffer = HC_Echo_Init();
    if (hc_buffer == NULL) Is_Error_report("HC Sensor - Echo Init error ERROR",ESP_FAIL,NULL);

    // Create Event Group
    IS_Event = xEventGroupCreate();

    // Send semaphore and buffer* to scan task
    xTaskCreatePinnedToCore(Task_Scan,"HC Scanner",8192,hc_buffer,1,NULL,1);
    
    // xTaskCreate(task_socket,"socket client",4096,NULL,1,NULL);
    // xTaskCreatePinnedToCore(task_socket,"Socket Client",8192,NULL,1,NULL,1);
}