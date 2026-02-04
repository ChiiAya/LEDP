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
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <math.h>
#include <fft.h>
#include "esp_dsp.h"
#include "driver.h"
static const char *TAG = "music_fft";
#define MINDB -100
#define MAXDB -6
#define SAMPLE_RATE 44100
#define CALIBARATION_FRAMES 60
#define NOISE_GATE_DB -52


int N = N_SAMPLES;
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
static uint8_t s_pixel_frame[FRAME_SIZE];
static uint16_t fft_index[WIDTH+1] = {0,1, 2, 3, 4, 6, 7, 9, 10, 13, 
    16, 19, 24, 29, 35, 43, 53, 64, 78, 96, 116, 142, 173,
     211, 257, 313, 381, 465, 566, 690, 840, 980,1023};
static uint8_t ColumnHeight[WIDTH] = {0,0,0,0,0,0,0,0};
static uint8_t JumpingBlock[WIDTH];

void initMusic(){
    memset(s_pixel_frame,0,FRAME_SIZE);

    // Generate hann window
    dsps_wind_hann_f32(wind, N);
    esp_err_t ret;
    ESP_LOGI(TAG, "Start FFT.");
    ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
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
    for(int i = 0;i < N_SAMPLES;i++){
        x1[i] = audiosource[i];
    }

    for (int i = 0 ; i < N ; i++) {
        y_cf[i * 2 + 0] = x1[i] * wind[i];
        y_cf[i * 2 + 1] = 0;
    }
    // FFT
    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_fft2r_fc32(y_cf, N);
    unsigned int end_b = dsp_get_cpu_cycle_count();
    // Bit reverse
    dsps_bit_rev_fc32(y_cf, N);

    for (int i = 0 ; i < N / 2 ; i++) {
        sum_y[i] = 10 * log10f((y_cf[i * 2 + 0] * y_cf[i * 2 + 0] + y_cf[i * 2 + 1] * y_cf[i * 2 + 1]) / N);
    }

    /*
     *================================  |
     *Now Generating The Pixels         |
     *================================  |
    */
    ESP_LOGW(TAG,"Generating Pixels");
    //噪音门
    for(int i = 0;i < N/2;i++){
        sum_y[i] = sum_y[i] < MINDB ? MINDB : sum_y[i];
        sum_y[i] = sum_y[i] > MAXDB ? MAXDB : sum_y[i];
    }
    //归一化
    for(int i = 0;i < N/2;i++){
        sum_y[i] = (sum_y[i] - MINDB) / (MAXDB - MINDB);
    }
    for(int i = 0;i < WIDTH;i++){
        temp = 0.0;
        for(int j = fft_index[i];j < fft_index[i+1];j++){
            temp += sum_y[j];
        }
        temp = temp/(fft_index[i+1]-fft_index[i]);
        if(temp < 0.125){
            ColumnHeight[i] = 0;
        }else if(temp < 0.25){ ColumnHeight[i] = 1;}
        else if(temp < 0.375){ ColumnHeight[i] = 2;}
        else if(temp < 0.5){ ColumnHeight[i] = 3;}
        else if(temp < 0.625){ ColumnHeight[i] = 4;}
        else if(temp < 0.75){ ColumnHeight[i] = 5;}
        else if(temp < 0.875){ ColumnHeight[i] = 6;}
        else{ ColumnHeight[i] = 7;}
    }

    for(int i = 0;i < WIDTH;i++){
        for(int j = 0;j<HEIGHT;j++){
            if(ColumnHeight[i] >= j){
                s_pixel_frame[i+j*WIDTH   ] = 75; //R,G,B
                s_pixel_frame[i+j*WIDTH + 1] = 0;
                s_pixel_frame[i+j*WIDTH + 2] = 0;
            }else{
                s_pixel_frame[i+j*WIDTH   ] = 0; //R,G,B
                s_pixel_frame[i+j*WIDTH + 1] = 0;
                s_pixel_frame[i+j*WIDTH + 2] = 0;
            }
        }
    }
    submitLEDFrame(s_pixel_frame);
    /*
    ================================
    Performance Analysis
    ================================
    */
    ESP_LOGI(TAG, "FFT for %i complex points take %i cycles", N, end_b - start_b);
    ESP_LOGI(TAG, "End Example.");
}