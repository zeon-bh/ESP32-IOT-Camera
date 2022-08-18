/* HC-SR04 Ultrasound sensor for the ESP32 IOT Surveillance camera
* Author : Bharath Ramachandran
* HC-SR04 driver program for object detection and ranging
*/

#ifndef HC_ULTRASOUND_H
#define HC_ULTRASOUND_H
#endif
#include "driver/rmt.h"
#include <esp_log.h>


// HC-SR04 Sensor definitions
#define HC_ECHO GPIO_NUM_12
#define HC_TRIG GPIO_NUM_13
#define RMT_CLK_DIV 100
#define RMT_TICK_10_us (80000000/RMT_CLK_DIV/100000)
#define HC_MIN_SIG_us 100 // Minimum ECHO signal duration which results in min distance measured around 2 cm
#define HC_MAX_SIG_us 80000 // Maximum ECHO signal duration which results in max distance measured around 4 meters
#define ITEM_DURATION(ticks) ((ticks & 0x7fff)*10/RMT_TICK_10_us)

// HC-SR04 Channel Configuration
#define HC_TRIG_CHANNEL RMT_CHANNEL_0
#define HC_ECHO_CHANNEL RMT_CHANNEL_1

// Trigger TX Buffer size
#define TRIG_MEM_BLOCK 1 // 1 block = 64x2 bytes
#define TRIG_TX_BUFFER 0

// Echo RX Buffer size
#define ECHO_MEM_BLOCK 2 // 1 block = 64x2 bytes
#define ECHO_RX_BUFFER 2000

// Detection range of US Sensor
typedef struct {
    float range_min;
    float range_max;
} US_detect_range_t;

// Init Functions
esp_err_t HC_Trig_Init(void);
RingbufHandle_t HC_Echo_Init(void);

float HC_Get_Range(RingbufHandle_t Echo_buffer);
