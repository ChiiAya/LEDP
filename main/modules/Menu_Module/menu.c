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
#include "esp_task_wdt.h"
#include "clock.h"
#include "led_strip_example_main.h"

static const char *TAG = "[Menu]";
static uint8_t current_task = 0;
SemaphoreHandle_t xSemaMenu = NULL;
SemaphoreHandle_t xSemaAnimationOver = NULL;
static uint8_t framebuffer[FRAME_SIZE] = {0};

void print_framebuffer(const uint8_t *fb, const char *label)
{
    if (!fb) {
        ESP_LOGE(TAG, "framebuffer is NULL");
        return;
    }

    if (label) {
        ESP_LOGI(TAG, "=== Framebuffer: %s ===", label);
    } else {
        ESP_LOGI(TAG, "=== Framebuffer ===");
    }

    for (int y = 0; y < LEDPanel_Height; y++) {
        printf("Row %d: ", y);
        for (int x = 0; x < LEDPanel_Width; x++) {
            int index = (y * LEDPanel_Width + x) * 3;
            uint8_t r = fb[index];
            uint8_t g = fb[index + 1];
            uint8_t b = fb[index + 2];
            
            if (r == 0 && g == 0 && b == 0) {
                printf(".");
            } else if (r > 0 && g == 0 && b == 0) {
                printf("R");
            } else if (g > 0 && r == 0 && b == 0) {
                printf("G");
            } else if (b > 0 && r == 0 && g == 0) {
                printf("B");
            } else {
                printf("*");
            }
        }
        printf("\n");
    }

    ESP_LOGI(TAG, "Legend: .=black, R=red, G=green, B=blue, *=mixed");
}

void Menu_Task(void *pvParameters){
    ESP_LOGI(TAG,"MenuConfigure");
    xSemaMenu = xSemaphoreCreateBinary();
    xSemaAnimationOver = xSemaphoreCreateBinary();
    while(1){
        xSemaphoreTake(xSemaMenu,portMAX_DELAY);
        ESP_LOGI(TAG,"EnterMenuConfigure");
        ESP_LOGI(TAG,"Fading Current Screen");
        get_latest_frame(framebuffer);
        fade(framebuffer,0,0,1,0.5);

        //print_framebuffer(framebuffer, "Original Frame");

        // 获取音频任务句柄
        TaskHandle_t hAudio = xTaskGetHandle("audio_viz");
        // 获取时钟任务句柄
        TaskHandle_t hClock = xTaskGetHandle("clock_task");
        
        ESP_LOGI(TAG,"==============CurrentTask == %d================",current_task);
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
            ESP_LOGW(TAG, "Task 'clock_task' not found");
        }

        xSemaphoreTake(xSemaAnimationOver,portMAX_DELAY);
        //xSemaphoreTake(xSemaAnimationOver,portMAX_DELAY);
        if (current_task == 0) {
            // 切换到状态 1: 恢复音频
            if (hAudio != NULL) {
                ESP_LOGI(TAG, "Resumed audio_viz");
                vTaskResume(hAudio);
            }else{
                ESP_LOGI(TAG, "Start audio_viz");
                ESP_ERROR_CHECK(startFFTtask());
            }
            hAudio = xTaskGetHandle("audio_viz");
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
