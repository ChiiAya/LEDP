// audio_input.h
#ifndef AUDIO_INPUT_H_
#define AUDIO_INPUT_H_

#include <stdint.h>
#include "esp_err.h"

// 配置常量（与 .c 文件一致）
#define FFT_SAMPLE_SIZE     2048
#define CONFIG_EXAMPLE_SAMPLE_RATE 44100

/**
 * @brief 初始化 PDM 麦克风（I2S RX）
 * 
 * 使用 GPIO 4 (CLK) 和 GPIO 5 (DATA)
 * 采样率：44.1kHz，16-bit，单声道
 * 
 * @return esp_err_t
 */
esp_err_t init_microphone(void);

/**
 * @brief 音频采集任务主函数（通常由 xTaskCreate 调用）
 * 
 * 此任务会：
 * - 持续从 I2S 读取音频
 * - 转换为 float 格式
 * - 调用 flash_audio_to_ledp() 更新 LED 像素
 * - （隐式）通过 submitLEDFrame 提交帧（需确保驱动已初始化）
 * @param pvParameters 未使用，传 NULL 即可
 */
void audio_input_task(void *pvParameters);

/**
 * @brief 获取最新原始音频数据指针（int16_t 格式）
 * 
 * 数据长度 = FFT_SAMPLE_SIZE
 * 仅用于调试或额外处理，**不建议在中断或高优先级任务中使用**
 * 
 * @return const int16_t* 指向内部缓冲区
 */
const int16_t* getAudioPointer(void);

/**
 * @brief 获取最新浮点音频数据指针（float 格式，范围 [-1.0, 1.0]）
 * 
 * 数据长度 = FFT_SAMPLE_SIZE
 * 
 * @return const float* 指向内部缓冲区
 */
const float* getFFTPointer(void);

#endif // AUDIO_INPUT_H_