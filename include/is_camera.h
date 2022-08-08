// ESP32 Camera module for video capture

#ifndef IS_CAMERA_H
#define IS_CAMERA_H
#endif
#include <esp_camera.h>
#include <esp_log.h>

// OV2640 Pin Definitions
#define CAM_D0	GPIO_NUM_5
#define CAM_D1	GPIO_NUM_18
#define CAM_D2	GPIO_NUM_19
#define CAM_D3	GPIO_NUM_26
#define CAM_D4	GPIO_NUM_36
#define CAM_D5	GPIO_NUM_39
#define CAM_D6	GPIO_NUM_34
#define CAM_D7	GPIO_NUM_35
#define CAM_XCLK GPIO_NUM_0
#define CAM_PCLK GPIO_NUM_22
#define CAM_VSYNC GPIO_NUM_25
#define CAM_HREF GPIO_NUM_23
#define CAM_SDA	GPIO_NUM_26
#define CAM_SCL	GPIO_NUM_27
#define CAM_PWR_PIN GPIO_NUM_32

// OV2640 Camera Settings
#define CAM_XCLK_FREQ 20000000
#define CAM_TIMER LEDC_TIMER_0
#define CAM_CHANNEL LEDC_CHANNEL_0
#define CAM_PIXEL_FORMAT PIXFORMAT_JPEG
#define CAM_FRAME_SIZE FRAMESIZE_SVGA
#define CAM_JPG_QUALITY 15 // 0-63 Lower value for better quality
#define CAM_FRAME_BF 2
#define CAM_GB_MODE CAMERA_GRAB_WHEN_EMPTY

esp_err_t IS_Camera_Init(void);
camera_fb_t* IS_Camera_Capture(void);
void IS_Return_buffer(camera_fb_t* frame_buffer);