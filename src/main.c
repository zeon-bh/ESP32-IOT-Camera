#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <status_led.h>
#include <hc_ultrasound.h>

void app_main() {
    Init_IS_LED_Timer();
    Init_IS_LED_Channel();

    // HC-SR04 Initialization
    HC_Trig_Init();
    RingbufHandle_t Echo_Rx_Buffer = HC_Echo_Init();
    US_detect_range_t detect_range = {};

    while(1){
        HC_Trigger_Pulse();
        float range = HC_Get_Range(Echo_Rx_Buffer);

        if(range < 50.0){
            IS_LED_Set_Mode(IS_Setup);
        }else{
            IS_LED_Set_Mode(IS_Error);
        }
        
        detect_range.range_min = range * 0.95;
        detect_range.range_max = range * 1.05;

        printf("Measured Distance MIN = %f cm | MAX = %f cm\n",detect_range.range_min,detect_range.range_max);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}