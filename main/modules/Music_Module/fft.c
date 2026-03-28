/*
初步设想：流有wifi，本地两种输入方式，写在audioinputstream里面
audiois进行处理后，绘制图像，发送给面板驱动
目标：基于fft的音频可视化
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <math.h>
#include <fft.h>
#include "esp_dsp.h"
#include "driver.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"

static const char *TAG = "music_fft";
#define MINDB -100
#define MAXDB_MAX MAXDB
#define MAXDB_MIN -20
#define MAXDB -6
#define SAMPLE_RATE 44100
#define CALIBARATION_FRAMES 60
#define NOISE_GATE_DB -100
#define decay_factor 0.9f
#define ADC_factor 0.05f
#define TARGET_FPS 120                          // 目标帧率（可调：60/80/100/120）
#define MIN_INTERVAL_US (1000000LL / TARGET_FPS) // T
#define SMOOTH_UP   0.8f
#define SMOOTH_DOWN 0.15f

static int64_t last_frame_time = 0;  // 上一帧时间（微秒）
static uint32_t frame_count = 0;     // 帧计数（用于平均 FPS）
static float avg_fps = 0.0f;         // 平均帧率

const static int N = N_SAMPLES;
// Input test array
__attribute__((aligned(16)))
float x1[N_SAMPLES];
// Window coefficients
__attribute__((aligned(16)))
float wind[N_SAMPLES];
// working complex array
__attribute__((aligned(16)))
float y_cf[N_SAMPLES * 2];

__attribute__((aligned(16)))
float sum_y[N_SAMPLES / 2];

static float temp = 0.0;
static uint8_t s_pixel_frame[FRAME_SIZE] = {0.0};
static float s_pixel_frame_f[FRAME_SIZE] = {0.0};
static uint16_t fft_index[LEDPanel_Width+1] = {0,1, 2, 3, 4, 6, 7, 9, 10, 13, 
    16, 19, 24, 29, 35, 43, 53, 64, 78, 96, 116, 142, 173,
     211, 257, 313, 381, 465, 566, 690, 840, 980,1023};
static float ColumnHeight[LEDPanel_Width] = {0,0,0,0,0,0,0,0};
static float Pre_avr_db = 0.0;

void initMusic(){
    memset(s_pixel_frame,0,FRAME_SIZE);

    // Generate hann window
    dsps_wind_hann_f32(wind, N);
    esp_err_t ret;
    ESP_LOGI(TAG, "Start FFT.");
    ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    ESP_LOGI(TAG, "N_SAMPLES = %d", N_SAMPLES);
    ESP_LOGI(TAG, "N = %d", N);
    ESP_LOGI(TAG, "CONFIG_DSP_MAX_FFT_SIZE = %d", CONFIG_DSP_MAX_FFT_SIZE); 
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT. Error = %i", ret);
        return;
    }

}

const uint8_t* getMusicPointer(){
    return s_pixel_frame;
}

void flash_audio_to_arrow(const float audiosource[N_SAMPLES])
{
    if (!audiosource) {
        ESP_LOGE(TAG, "audiosource is NULL!");
        return;
    }
    // 检查对齐
    if (((uint32_t)audiosource & 0xF) != 0) {
        ESP_LOGE(TAG, "audiosource not 16-byte aligned: %p", audiosource);
        return;
    }

    for (int i = 0 ; i < N ; i++) {
        y_cf[i * 2 + 0] = audiosource[i] * wind[i];
        y_cf[i * 2 + 1] = 0;
    }
    // FFT
    //unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_fft2r_fc32(y_cf, N);
    //unsigned int end_b = dsp_get_cpu_cycle_count();
    // Bit reverse
    dsps_bit_rev_fc32(y_cf, N);
    //unsigned int end_b_bitr = dsp_get_cpu_cycle_count();


    for (int i = 0 ; i < N / 2 ; i++) {
        sum_y[i] = 10 * log10f((y_cf[i * 2 + 0] * y_cf[i * 2 + 0] + y_cf[i * 2 + 1] * y_cf[i * 2 + 1]) / N);
    }

    //动态增益计算平均功率
    float db_avr = 0.0;
    for(int i = 0;i < N/2;i++){
        db_avr += sum_y[i];
    }
    db_avr /= N/2;
    Pre_avr_db = ADC_factor * db_avr + (1 - ADC_factor) * Pre_avr_db;
    //动态调整噪音门
    float min_db = MINDB;
    float max_db = Pre_avr_db + 20.0f;

    //防止大突变
    if(max_db > MAXDB_MAX) max_db = MAXDB_MAX;
    if(max_db < MAXDB_MIN) max_db = MAXDB_MIN;

    //归一化
    for(int i = 0;i < N/2;i++){
        float clamped_db = fmaxf(sum_y[i], min_db);
        clamped_db = fminf(clamped_db, max_db);
        sum_y[i] = (clamped_db - min_db) / (max_db - min_db);
        
    }

    //决定高度
    for(int i = 0;i < LEDPanel_Width - 1;i++){
        temp = 0.0;
        for(int j = fft_index[i];j < fft_index[i+1];j++){
            temp += sum_y[j];
        }
        temp = temp/(fft_index[i+1]-fft_index[i]);
        float height = temp * LEDPanel_Height;
        if (height > ColumnHeight[i]) {
            // 上升阶段：使用 SMOOTH_UP，保持打击感
            ColumnHeight[i] = SMOOTH_UP * height + (1.0f - SMOOTH_UP) * ColumnHeight[i];
        } else {
            // 下降阶段：使用 SMOOTH_DOWN，让回落变丝滑
            ColumnHeight[i] = SMOOTH_DOWN * height + (1.0f - SMOOTH_DOWN) * ColumnHeight[i];
        }
    }

    //在上一帧的基础上衰减
    for(int i = 0;i < LEDPanel_Width*LEDPanel_Height*3;i++){
        s_pixel_frame_f[i] *= decay_factor;
    }
    
    //绘制
    for(int i = LEDPanel_Width - 1;i >= 0;i--){
        uint32_t hue = i * (300 / (LEDPanel_Width - 1)); // 左(低频)=0°(红), 右(高频)=300°(紫)
        uint32_t value = 20;//20 + (uint32_t)(80.0f * fminf(fmaxf(sum_y[i], 0.0f), 1.0f));
        uint32_t saturation = 100;
        uint32_t r, g, b;
        float scale = 64.0f / 255.0f;
        led_strip_hsv2rgb(hue, saturation, value, &r, &g, &b);

        for(int j = 0;j<LEDPanel_Height;j++){
            if((uint8_t)ColumnHeight[i] >= j){
                s_pixel_frame_f[(i+(LEDPanel_Height - j - 1)*LEDPanel_Width)*3    ] = r * scale; //R,G,B
                s_pixel_frame_f[(i+(LEDPanel_Height - j - 1)*LEDPanel_Width)*3 + 1] = g * scale;
                s_pixel_frame_f[(i+(LEDPanel_Height - j - 1)*LEDPanel_Width)*3 + 2] = b * scale;
            }
        }
    }
    //unsigned int end_b_paint = dsp_get_cpu_cycle_count();
    //转为uint8
    for(int i = 0;i < LEDPanel_Width*LEDPanel_Height*3;i++){
        s_pixel_frame[i] = s_pixel_frame_f[i];
    }
    //提交至队列
    int64_t now_time = esp_timer_get_time();
    int64_t ttime = last_frame_time;
    if (now_time - last_frame_time >= MIN_INTERVAL_US) {
        submitLEDFrame(s_pixel_frame);
        last_frame_time = now_time;
    }
    
    /*
    ================================
    Performance Analysis
    ================================
    */
    // === 帧率统计 ===
    if (last_frame_time != 0) {
        int64_t delta_us = now_time - ttime;
        float instant_fps = 1000000.0f / delta_us; // 转为 FPS

        // 指数平滑平均 FPS（避免抖动）
        avg_fps = 0.9f * avg_fps + 0.1f * instant_fps;

        // 每 30 帧打印一次（避免日志刷屏）
        if (++frame_count >= 120) {
            ESP_LOGI(TAG, "Avg FPS: %.1f", avg_fps);
            frame_count = 0;
        }
    }
    
}