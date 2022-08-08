#include <stdio.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <is_wifi.h>
#include <esp_http_client.h>
#include <status_led.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <is_camera.h>

#define SERVER_IP CONFIG_IPV4_ADDR
#define SERVER_PORT CONFIG_PORT
#define RETRY_MAX 10

static const char* TAG = "Socket";

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
            // Send the buffer data
            err = send(sock, fb->buf, fb->len, 0);
            bool isJPG = frame2jpg(fb,80,fb->buf,fb->len);
            if (!isJPG) ESP_LOGE(TAG,"Camera Frame buffer not converted to JPG");

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

void app_main() {
    Init_IS_LED();
    IS_LED_Set_Mode(IS_Setup);
    Init_WiFi();
    vTaskDelay(pdMS_TO_TICKS(2500)); // Wait 2.5 seconds for wifi driver to configure and connect to AP
    IS_LED_Set_Mode(IS_Scan);

    IS_Camera_Init();
    
    // xTaskCreate(task_socket,"socket client",4096,NULL,1,NULL);
    xTaskCreatePinnedToCore(task_socket,"Socket Client",8192,NULL,1,NULL,1);
}