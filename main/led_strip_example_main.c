/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "clock.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "modules/LEDPanel_Driver/driver.h"
#include "driver.h"
#include "modules/Music_Module/audio.h"
#include "fft.h"
#include "esp_freertos_hooks.h"
#include "freertos/task.h"

#define TASK_INDEX_IDLE_RUN 0  // 使用索引 0 表示 "IDLE 任务已运行"
static const char *TAG = "MAIN";

TaskHandle_t mainTaskHandle = 0;
DRAM_ATTR static uint8_t waiting_for_idle = 0;
DRAM_ATTR uint8_t idle_task_was_run = 0;

static bool idle_hook(void)
{
    idle_task_was_run = 1;
    if (waiting_for_idle) {
        waiting_for_idle = 0;  // we only want to notify once
        xTaskNotifyIndexed(mainTaskHandle, TASK_INDEX_IDLE_RUN, 0, eNoAction);
        return false;  // do not idle (i.e. do not wait for interrupt) as we want main task to run
    }
    return true;  // allows idle task to idle (i.e. to wait for interrupt)
}

void app_main(void)
{
    mainTaskHandle = xTaskGetCurrentTaskHandle();
    esp_register_freertos_idle_hook_for_cpu(idle_hook, (UBaseType_t)xPortGetCoreID());
    ESP_LOGI(TAG, "MainFunction Booted");

    //driver init
    ESP_LOGI(TAG,"DriverInit");
    initRMT();//taskcreated
    clearPanel();
// 测试全红
    ESP_LOGI(TAG,"SelfTest");
    uint8_t test_frame[FRAME_SIZE];
    memset(test_frame, 0, FRAME_SIZE);
    for (int j = 0; j < 32; j++) {
        int idx = (j) * 3;//y * Width + x
        test_frame[idx + 0] = 10; // R
        test_frame[idx + 1] = 0;   // G
        test_frame[idx + 2] = 0;   // B
    }
    submitLEDFrame(test_frame);
    vTaskDelay(pdMS_TO_TICKS(500));
    clearPanel();

    for (int i = 0; i < 8; i++) {
        int idx = (i * 32 + 0) * 3;
        test_frame[idx + 0] = 0;   // R
        test_frame[idx + 1] = 10; // G
        test_frame[idx + 2] = 0;   // B → 显示绿色，便于区分
    }
    submitLEDFrame(test_frame);
    vTaskDelay(pdMS_TO_TICKS(500));
    clearPanel();
    clock_module_init();
    //audio init
    // initMusic();
    // init_microphone();//taskcreated
}