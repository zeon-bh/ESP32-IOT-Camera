/* LED Status indicator for the ESP32 IOT Surveillance camera
* Author : Bharath Ramachandran
* This program allows the ESP32 to use its built-in LED to report the status of the device
* during operation.
*/

#ifndef STATUS_LED_H
#define STATUS_LED_H
#endif
#include <driver/ledc.h>
#include <esp_log.h>

// US LED Definitions
#define IS_LED GPIO_NUM_33 // In-Built Red LED-1
// #define IS_LED GPIO_NUM_4 // In-Built Flash LED
#define IS_LED_TIMER LEDC_TIMER_1
#define IS_LED_CHAN LEDC_CHANNEL_1
#define IS_LED_SCAND 64 // Duty cycle
#define IS_LED_SETUPF 8
#define IS_LED_SCANF 30
#define IS_LED_ERRORF 1

typedef enum {
    IS_Setup = 0,
    IS_Scan,
    IS_Error
} IS_LED_Mode;


// Initialize Status LED Module
void Init_IS_LED(void);

// IS LED modes
void IS_LED_Set_Mode(IS_LED_Mode);