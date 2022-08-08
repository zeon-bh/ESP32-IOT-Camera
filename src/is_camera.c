#include <is_camera.h>

static const char* TAG = "IS_Camera";

static const camera_config_t is_camera_config = {
    .pin_pwdn = CAM_PWR_PIN,
    .pin_reset = -1,
    .pin_sscb_scl = CAM_SCL,
    .pin_sscb_sda = CAM_SDA,
    .pin_href = CAM_HREF,
    .pin_vsync = CAM_VSYNC,
    .pin_pclk = CAM_PCLK,
    .pin_xclk = CAM_XCLK,
    .pin_d7 = CAM_D7,
    .pin_d6 = CAM_D6,
    .pin_d5 = CAM_D5,
    .pin_d4 = CAM_D4,
    .pin_d3 = CAM_D3,
    .pin_d2 = CAM_D2,
    .pin_d1 = CAM_D1,
    .pin_d0 = CAM_D0,

    .xclk_freq_hz = CAM_XCLK_FREQ,
    .ledc_timer = CAM_TIMER,
    .ledc_channel = CAM_CHANNEL,
    .pixel_format = CAM_PIXEL_FORMAT,
    .frame_size = CAM_FRAME_SIZE,
    .jpeg_quality = CAM_JPG_QUALITY,
    .fb_count = CAM_FRAME_BF,
    .grab_mode = CAM_GB_MODE
};


esp_err_t IS_Camera_Init(){
    esp_err_t ret = esp_camera_init(&is_camera_config);
    if (ret != ESP_OK){
        ESP_LOGE(TAG,"Camera initialization failed!!!!");
        ESP_ERROR_CHECK(ret);
    }
    return ret;
}

camera_fb_t* IS_Camera_Capture(){
    camera_fb_t* is_fb = esp_camera_fb_get();
    if(is_fb == NULL){
        ESP_LOGE(TAG,"Frambe buffer not acquired!!!");
        return NULL;
    }
    return is_fb;
}

void IS_Return_buffer(camera_fb_t* frame_buffer){
    esp_camera_fb_return(frame_buffer);
}