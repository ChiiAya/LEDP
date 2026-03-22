#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include <math.h>
#include <esp_log.h>
#include <menu.h>
#include <AnimationSet.h>
#include <semaphore.h>
#include <driver.h>

static const char *TAG = "[Menu]";
static uint8_t current_task = 0;
SemaphoreHandle_t xSemaMenu = NULL;
SemaphoreHandle_t xSemaAnimationOver = NULL;
static uint8_t framebuffer[FRAME_SIZE] = {0};
void Menu_Task(void *pvParameters){
    ESP_LOGI(TAG,"MenuConfigure");
    xSemaMenu = xSemaphoreCreateBinary();
    xSemaAnimationOver = xSemaphoreCreateBinary();
    while(1){
        xSemaphoreTake(xSemaMenu,portMAX_DELAY);
        ESP_LOGI(TAG,"EnterMenuConfigure");
        ESP_LOGI(TAG,"Fading Current Screen");
        get_latest_frame(framebuffer);
        fade(framebuffer,0,0,1,1);

        // 获取音频任务句柄
        TaskHandle_t hAudio = xTaskGetHandle("audio_viz");
        // 获取时钟任务句柄
        TaskHandle_t hClock = xTaskGetHandle("clock");
        
        // 挂起音频任务 (如果存在)
        if (hAudio != NULL) {
            vTaskSuspend(hAudio);
            ESP_LOGI(TAG, "Suspended audio_viz");
        } else {
            ESP_LOGW(TAG, "Task 'audio_viz' not found");
        }

        // 挂起时钟任务 (如果存在)
        if (hClock != NULL) {
            vTaskSuspend(hClock);
            ESP_LOGI(TAG, "Suspended clock");
        } else {
            ESP_LOGW(TAG, "Task 'clock' not found");
        }

        xSemaphoreTake(xSemaAnimationOver,pdMS_TO_TICKS(2000));
        if (current_task == 0) {
            // 切换到状态 1: 恢复音频
            if (hAudio != NULL) {
                vTaskResume(hAudio);
                ESP_LOGI(TAG, "Resumed audio_viz");
            }
            current_task = 1;
        } else {
            // 切换到状态 0: 恢复时钟
            if (hClock != NULL) {
                vTaskResume(hClock);
                ESP_LOGI(TAG, "Resumed clock");
            }
            current_task = 0;
        }
    }
}
