#include "driver/rmt.h"

// HC-SR04 Sensor definitions
#define HC_ECHO GPIO_NUM_32
#define HC_TRIG GPIO_NUM_33
#define RMT_CLK_DIV 100
#define RMT_TICK_10_us (80000000/RMT_CLK_DIV/100000)
#define HC_MIN_SIG_us 100 // Minimum ECHO signal duration which results in min distance measured around 2 cm
#define HC_MAX_SIG_us 80000 // Maximum ECHO signal duration which results in max distance measured around 4 meters
#define ITEM_DURATION(ticks) ((ticks & 0x7fff)*10/RMT_TICK_10_us)

// Detection range of US Sensor
typedef struct {
    float range_min;
    float range_max;
} US_detect_range_t;

// Init Functions
void HC_Trig_Init(void);
void HC_Echo_Init(void);
