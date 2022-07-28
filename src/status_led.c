# include <status_led.h>

// IS LED logging TAG
static const char* TAG = "IS_LED";

// Initialize the LEDC Timer using the REF_CLK [1 Mhz]
void Init_IS_LED_Timer(void){
    ledc_timer_config_t led_blink = {
        .duty_resolution = LEDC_TIMER_15_BIT,
        .clk_cfg = LEDC_USE_REF_TICK,
        .freq_hz = 1,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = IS_LED_TIMER
    };
    esp_err_t ret = ledc_timer_config(&led_blink);

    if(ret != ESP_OK){
        ESP_LOGE(TAG,"LEDC Timer initialization failed!!! | Timer No. = %d",IS_LED_TIMER);
        ESP_ERROR_CHECK(ret);
    }
}


// Initialize the LEDC Channel with HIGH_SPEED_MODE
void Init_IS_LED_Channel(void){
    ledc_channel_config_t led_blink_chan = {
        .gpio_num = IS_LED,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = IS_LED_CHAN,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = IS_LED_TIMER,
        .duty = 16383,
    };
    esp_err_t ret = ledc_channel_config(&led_blink_chan);

    if(ret != ESP_OK){
        ESP_LOGE(TAG,"LEDC Channel initialization failed!!! | Channel No. = %d",IS_LED_CHAN);
        ESP_ERROR_CHECK(ret);
    }
}

// Set Blink pattern for the in-built led for status feedback
void IS_LED_Set_Mode(IS_LED_Mode mode){
    esp_err_t ret = ESP_OK;
    switch (mode)
    {
    case IS_Setup:
        ret = ledc_set_freq(LEDC_HIGH_SPEED_MODE,IS_LED_TIMER,IS_LED_SETUPF);
        break;

    case IS_Scan:
        ret = ledc_set_freq(LEDC_HIGH_SPEED_MODE,IS_LED_TIMER,IS_LED_SCANF);
        if (ret != ESP_OK) break;
        ret = ledc_set_duty(LEDC_HIGH_SPEED_MODE,IS_LED_CHAN,IS_LED_SCAND);
        break;

    case IS_Error:
        ret = ledc_set_freq(LEDC_HIGH_SPEED_MODE,IS_LED_TIMER,IS_LED_ERRORF);
        break;
    
    default:
        ESP_LOGE(TAG,"LED Mode [%d] not defined.",mode);
        ret = ESP_FAIL;
        break;
    }

    if(ret != ESP_OK){
        ESP_LOGE(TAG,"LED Set mode failed | mode = %d",mode);
        ESP_ERROR_CHECK(ret);
    }
}