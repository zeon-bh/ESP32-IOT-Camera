#include <stdio.h>
#include <status_led.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void app_main() {
    Init_IS_LED_Timer();
    Init_IS_LED_Channel();

    while(1){
        for(int mode = IS_Setup; mode <= IS_Error; mode++){
            IS_LED_Set_Mode(mode);
            printf("LED Mode = %d\n",mode);
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }
}