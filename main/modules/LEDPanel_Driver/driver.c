/*
driver receives messages from other modules and sends them to the LEDPanel 
some of the code is from esp-idf examples
驱动模块接受可读的二维数组并且将其作为图像发送给LED面板
一部分代码源自于ESP_IDF的官方样例
-----------------------------------------------------------------------------
by ChiiAya 20251231
*/

#include "driver.h"
#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      0
#define EXAMPLE_CHASE_SPEED_MS      10
#define DEBUG 1

static const char *TAG = "driver";

static uint8_t led_strip_pixels[LEDPanel_Height * LEDPanel_Width * 3];

static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;
static rmt_transmit_config_t tx_config = {
    .loop_count = 0, // no transfer loop
};
//controller

void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

esp_err_t initRMT()//抄的样例
{
    ESP_LOGI(TAG, "Initialize RMT peripheral");
    if(led_chan != NULL){
        ESP_LOGI(TAG, "LED Channel already exists");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));//there is some worry

    ESP_LOGI(TAG, "Install led strip encoder");
    led_encoder = NULL;
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));

    ESP_LOGI(TAG, "LED PanelDriver ready to receive commands");
    return ESP_OK;
}

esp_err_t paintLEDPanel(ledp_pixel_t pixel[LEDPanel_Height][LEDPanel_Width])//receive pixels接受像素
{
    //fill the pixels填充像素
    int index = 0;
    for(int i = 0; i < LEDPanel_Height; i++){//为了适配S型走线
        for(int j = 0; j < LEDPanel_Width; j++){
            if(i % 2 == 0){
                led_strip_pixels[index * 3 + 0] = pixel[i][j].red;
                led_strip_pixels[index * 3 + 1] = pixel[i][j].green;
                led_strip_pixels[index * 3 + 2] = pixel[i][j].blue;
            }else{
                led_strip_pixels[index * 3 + 0] = pixel[i][LEDPanel_Width-1-j].red;
                led_strip_pixels[index * 3 + 1] = pixel[i][LEDPanel_Width-1-j].green;
                led_strip_pixels[index * 3 + 2] = pixel[i][LEDPanel_Width-1-j].blue;
            }
            index++;
        }
    }

    #ifdef DEBUG
            //打印缓冲区
            int index_Debug = 0;
            for(int i = 0; i < LEDPanel_Height; i++){
                for(int j = 0; j < LEDPanel_Width; j++){
                    printf("\033[38;2;%d;%d;%dm", led_strip_pixels[index_Debug*3],led_strip_pixels[index_Debug*3+1],led_strip_pixels[index_Debug*3+2]);
                    printf("█");
                    printf("\033[0m");
                }
                printf("\n");
            }
    #endif
    ESP_LOGI(TAG,"Begin Send A Frame");
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
    ESP_LOGI(TAG,"Send Frame Over");  

    return ESP_OK;
    //TaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
    // paint

    /*the origin example codes
    while (1) {
        for (int i = 0; i < 3; i++) {
            for (int j = i; j < EXAMPLE_LED_NUMBERS; j += 3) {
                // Build RGB pixels
                hue = j * 360 / EXAMPLE_LED_NUMBERS + start_rgb;
                led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
                led_strip_pixels[j * 3 + 0] = green;
                led_strip_pixels[j * 3 + 1] = blue;
                led_strip_pixels[j * 3 + 2] = red;
            }
            // Flush RGB values to LEDs
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
            memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
        }
        start_rgb += 60;
    }
    the origin example codes
    */
}

esp_err_t clearPanel()//简单的清屏
{
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
    //vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
    return ESP_OK;
}