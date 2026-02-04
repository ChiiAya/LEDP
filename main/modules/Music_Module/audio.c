/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/* I2S Digital Microphone Recording Example */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_pdm.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "fft.h"

static const char *TAG = "pdm_rec_example";

#define FFT_SAMPLE_SIZE     2048
#define NUM_CHANNELS        (1) // For mono recording only!
#define SAMPLE_SIZE         (CONFIG_EXAMPLE_BIT_SAMPLE * FFT_SAMPLE_SIZE)
#define I2S_READ_LEN        (FFT_SAMPLE_SIZE * sizeof(int16_t)) 
#define BYTE_RATE           (CONFIG_EXAMPLE_SAMPLE_RATE * (CONFIG_EXAMPLE_BIT_SAMPLE / 8)) * NUM_CHANNELS
#define CONFIG_EXAMPLE_I2S_CLK_GPIO  4
#define CONFIG_EXAMPLE_I2S_DATA_GPIO    5
#define CONFIG_EXAMPLE_SAMPLE_RATE 44100
// When testing SD and SPI modes, keep in mind that once the card has been
// initialized in SPI mode, it can not be reinitialized in SD mode without
// toggling power to the card.

i2s_chan_handle_t rx_handle = NULL;

static int16_t i2s_readraw_buff[FFT_SAMPLE_SIZE];
static float fft_buff[FFT_SAMPLE_SIZE];
size_t bytes_read;

int16_t* getAudioPointer(){
    return i2s_readraw_buff;
}

float* getFFTPointer(){
    return fft_buff;
}

void audio_input_task(void *pvParameters)
{
    size_t bytes_read;
    const float scale = 1.0f / 32768.0f; // int16 转 float 的缩放因子

    while (1) {
        // 从 I2S 读取原始 PCM 数据（阻塞直到填满 buffer）
        esp_err_t ret = i2s_channel_read(
            rx_handle,
            (char *)i2s_readraw_buff, // 目标缓冲区
            I2S_READ_LEN,             // 要读取的字节数
            &bytes_read,              // 实际读取字节数
            portMAX_DELAY             // 永不超时（实时流必需）
        );

        if (ret == ESP_OK && bytes_read == I2S_READ_LEN) {
            // 转换为 float [-1.0, 1.0]
            for (int i = 0; i < FFT_SAMPLE_SIZE; i++) {
                fft_buff[i] = i2s_readraw_buff[i] * scale;
            }
            flash_audio_to_arrow(fft_buff);

        } else {
            ESP_LOGE(TAG, "I2S read failed!");
            vTaskDelay(pdMS_TO_TICKS(10)); // 防止死循环
        }
    }
}

void init_microphone(void)
{
#if SOC_I2S_SUPPORTS_PDM2PCM
    ESP_LOGI(TAG, "Receive PDM microphone data in PCM format");
#else
    ESP_LOGI(TAG, "Receive PDM microphone data in raw PDM format");
#endif  // SOC_I2S_SUPPORTS_PDM2PCM
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(CONFIG_EXAMPLE_SAMPLE_RATE),
        /* The default mono slot is the left slot (whose 'select pin' of the PDM microphone is pulled down) */
#if SOC_I2S_SUPPORTS_PDM2PCM
        .slot_cfg = I2S_PDM_RX_SLOT_PCM_FMT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
#else
        .slot_cfg = I2S_PDM_RX_SLOT_RAW_FMT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
#endif
        .gpio_cfg = {
            .clk = CONFIG_EXAMPLE_I2S_CLK_GPIO,
            .din = CONFIG_EXAMPLE_I2S_DATA_GPIO,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    xTaskCreate(
        audio_input_task,
        "audio_viz",
        4096,               // 堆栈大小（FFT + 日志可能需要较大）
        NULL,
        configMAX_PRIORITIES - 2, // 较高优先级（避免音频卡顿）
        NULL
    );
}