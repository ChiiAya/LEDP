/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "modules/LEDPanel_Driver/driver.h"
#include "driver.h"
#include "modules/Music_Module/audio.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "MainFunction Booted");

    //driver init
    ESP_LOGI(TAG,"DriverInit");
    initRMT();//taskcreated
    clearPanel();
    ESP_LOGI(TAG,"FinishDriverInition");
    //audio init
    
    init_microphone();//taskcreated

}