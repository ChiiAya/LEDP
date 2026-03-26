/*
 * LEDP
 * By ChiiAya
 */
#include "clock.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "esp_timer.h"
#include "modules/LEDPanel_Driver/driver.h"
#include "driver.h"
#include "modules/Music_Module/audio.h"
#include "fft.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "menu.h"
#include "nvs_flash.h"
#include "smartconfig.h"

uint8_t StartScreen[32] = {
    0x00, 0x00, 0x00, 0x00, 0x7E, 0x02, 0x02, 0x02, 
    0x02, 0x00, 0x3C, 0x4A, 0x4A, 0x4A, 0x32, 0x00, 
    0x00, 0x7E, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x7E, 
    0x48, 0x48, 0x48, 0x30, 0x00, 0x00, 0x00, 0x00
};

static const char *TAG = "MAIN";
static int64_t last_debounce_time = 0; // 记录上次有效中断的时间
const int64_t DEBOUNCE_DELAY_MS = 500;  // 消抖延迟 500ms 
//一般来说，开机后应该进入时钟模式，通过旋钮或按键触发中断后再进入菜单
//鉴于fft线程对主CPU的占用之大，理应给其一个空闲线程来喂狗（目前关闭了WatchDog）（前提是不会影响性能）

static void IRAM_ATTR HandleGPIOInterruption(void* arg) {
    int64_t current_time = esp_timer_get_time() / 1000; // 获取当前时间
    if (current_time - last_debounce_time < DEBOUNCE_DELAY_MS) {
        return; 
    }
    last_debounce_time = current_time;

    BaseType_t xHigherPriTaskWoken = pdFALSE;
    if (xSemaMenu != NULL) {
        xSemaphoreGiveFromISR(xSemaMenu, &xHigherPriTaskWoken);
    }
    if (xHigherPriTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

void configure_gpio_interrupt() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ConfigButton),  // 选择 GPIO 4
        .mode = GPIO_MODE_INPUT,               // 输入模式
        .intr_type = GPIO_INTR_NEGEDGE,        // 下降沿触发
        .pull_up_en = GPIO_PULLUP_ENABLE,      // 启用上拉电阻（可选）
    };
    gpio_config(&io_conf);
}

esp_err_t startFFTtask(){
    xTaskCreate(
        audio_input_task,
        "audio_viz",
        4096 * 2, // 堆栈大小（FFT + 日志可能需要较大）
        NULL,
        PRIORITY_DRAWING_TASK, // 较高优先级（避免音频卡顿）
        NULL);
        return ESP_OK;
}

esp_err_t startAudioInputTask(){
    xTaskCreate(
        audio_input_task,
        "audio_viz",
        4096 * 2, // 堆栈大小（FFT + 日志可能需要较大）
        NULL,
        PRIORITY_DRAWING_TASK, // 较高优先级（避免音频卡顿）
        NULL);
        return ESP_OK;
}

esp_err_t drawStartingScreen(uint8_t *framebuffer){
    uint8_t black_frame[FRAME_SIZE];
    memset(framebuffer, 0, FRAME_SIZE);
    memset(black_frame, 0, FRAME_SIZE);
    for(int i = 0; i < LEDPanel_Width; i++){
        for(int j = 0; j < LEDPanel_Height; j++){
            uint8_t pixel_index = (i * LEDPanel_Height + j) * 3;
            if((StartScreen[i] & (1 << j)) != 0){
                framebuffer[pixel_index] = 20;
                framebuffer[pixel_index+1] = 20;
                framebuffer[pixel_index+2] = 20;
            }
        }
    };
    for(uint8_t alpha = 0; alpha < 255; alpha+=5){
        submitBlendedFrame(framebuffer, black_frame, alpha);
        vTaskDelay(pdMS_TO_TICKS(50));
    };
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "MainFunction Booted");

    //driver init
    ESP_LOGI(TAG,"DriverInit");
    initRMT();//taskcreated
    clearPanel();
    // 测试全红
    ESP_LOGI(TAG,"SelfTest");
    uint8_t test_frame[FRAME_SIZE];
    memset(test_frame, 0, FRAME_SIZE);
    // for (int j = 0; j < 32; j++) {
    //     int idx = (j) * 3;//y * Width + x
    //     test_frame[idx + 0] = 10; // R
    //     test_frame[idx + 1] = 0;   // G
    //     test_frame[idx + 2] = 0;   // B
    // }
    // submitLEDFrame(test_frame);
    // vTaskDelay(pdMS_TO_TICKS(500));
    // clearPanel();

    // for (int i = 0; i < 8; i++) {
    //     int idx = (i * 32 + 0) * 3;
    //     test_frame[idx + 0] = 0;   // R
    //     test_frame[idx + 1] = 10; // G
    //     test_frame[idx + 2] = 0;   // B → 显示绿色，便于区分
    // }
    // submitLEDFrame(test_frame);
    // vTaskDelay(pdMS_TO_TICKS(500));
    // clearPanel();
    
    //以上是自检

    drawStartingScreen(test_frame);
    //配置中断
    configure_gpio_interrupt();
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ConfigButton,HandleGPIOInterruption,(void *)ConfigButton);

    //配网任务
    wifi_autoconfigure();
    //create menu task
    xTaskCreate(Menu_Task, "MenuTask", 4096, NULL, PRIORITY_MENU_TASK, NULL);
    //create clock task
    clock_module_init();
    //FFT_task
    initMusic();
    init_microphone();
    //startFFTtask();
}