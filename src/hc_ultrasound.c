#include <hc_ultrasound.h>

// ESP Logging TAG
static const char* TAG = "HC_Ultrasound";

// Initializes the RMT Tx module
void HC_Trig_Init(){
    esp_err_t ret;
    rmt_config_t hc_trig_conf = {
        .rmt_mode = RMT_MODE_TX,
        .channel = HC_TRIG_CHANNEL,
        .gpio_num = HC_TRIG,
        .mem_block_num = TRIG_MEM_BLOCK,
        .clk_div = RMT_CLK_DIV,
        .tx_config.carrier_en = false,
        .tx_config.loop_en = false,
        .tx_config.carrier_duty_percent = 50,
        .tx_config.carrier_freq_hz = 5000,
        .tx_config.carrier_level = 1,
        .tx_config.idle_level = 0,
        .tx_config.idle_output_en = true
    };
    // Configure the RMT Module as TX on channel 0
    ret = rmt_config(&hc_trig_conf);
    if (ret != ESP_OK){
        ESP_LOGE(TAG,"HC Trigger configuration on GPIO = %d, RMT_TX_CHANNEL = %d failed!!!",HC_TRIG,HC_TRIG_CHANNEL);
        ESP_ERROR_CHECK(ret);
    }

    // Install the TX Driver
    ret = rmt_driver_install(HC_TRIG_CHANNEL,TRIG_TX_BUFFER,0);
    if (ret != ESP_OK){
        ESP_LOGE(TAG,"HC Trigger driver installation failed !!! check error details....");
        ESP_ERROR_CHECK(ret);
    }
}

// Initializes the RMT Rx module and returns an handle to a ring-buffer which stores the recieved signal data
// Returns a pointer to a RingBuffer handle
RingbufHandle_t HC_Echo_Init(){
    esp_err_t ret;
    rmt_config_t hc_echo_conf = {
        .rmt_mode = RMT_MODE_RX,
        .channel = HC_ECHO_CHANNEL,
        .gpio_num = HC_ECHO,
        .mem_block_num = ECHO_MEM_BLOCK,
        .clk_div = RMT_CLK_DIV,
        .rx_config.filter_en = false,
        .rx_config.idle_threshold = HC_MAX_SIG_us/10 * (RMT_TICK_10_us)
        //.rx_config.idle_threshold = HC_MAX_SIG_us
    };
    // Configure the RMT Module as RX on channel 1
    ret = rmt_config(&hc_echo_conf);
    if (ret != ESP_OK){
        ESP_LOGE(TAG,"HC Echo configuration on GPIO = %d, RMT_RX_CHANNEL = %d failed!!!",HC_ECHO,HC_ECHO_CHANNEL);
        ESP_ERROR_CHECK(ret);
        return NULL;
    }

    // Install the RX Driver
    ret = rmt_driver_install(HC_ECHO_CHANNEL,ECHO_RX_BUFFER,0);
    if (ret != ESP_OK){
        ESP_LOGE(TAG,"HC Echo driver installation failed !!! check error details....");
        ESP_ERROR_CHECK(ret);
        return NULL;
    }

    // Initialize RingBuffer to store RX Signal values and start rmt_rx
    RingbufHandle_t Echo_buffer = NULL;
    ESP_ERROR_CHECK(rmt_get_ringbuf_handle(HC_ECHO_CHANNEL, &Echo_buffer));
    ESP_ERROR_CHECK(rmt_rx_start(HC_ECHO_CHANNEL,1));

    return Echo_buffer;
}

// Generate a 10us Pulse to trigger the HC-SR04 Sensor
void HC_Trigger_Pulse(){
    rmt_item32_t hc_trig = {
        .level0 = 1,
        .duration0 = RMT_TICK_10_us,
        .level1 = 0,
        .duration1 = RMT_TICK_10_us
    };

    ESP_ERROR_CHECK(rmt_write_items(HC_TRIG_CHANNEL,&hc_trig,1,true));
    ESP_ERROR_CHECK(rmt_wait_tx_done(HC_TRIG_CHANNEL,portMAX_DELAY));
}

// Read from RMT Rx Buffer and calculate the range measured by the sensor
// Returns -1.0 if no data is read from the buffer after timeout
float HC_Get_Range(RingbufHandle_t Echo_buffer){
    rmt_item32_t * rx_Item;
    size_t echo_item_size = 0;
    float distance;

    rx_Item = (rmt_item32_t*)xRingbufferReceive(Echo_buffer,&echo_item_size,1000);

    if(rx_Item != NULL){
        // Get distance in centimeters
        distance = 34029 * ITEM_DURATION(rx_Item->duration0) / (2000000);
        vRingbufferReturnItem(Echo_buffer,(void*)rx_Item);
        return distance;
    }else{
        ESP_LOGE(TAG,"Rx Ring Buffer is Empty!!!");
        return -1.0;
    }
}