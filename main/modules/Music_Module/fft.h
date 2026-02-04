// music.h

#ifndef MUSIC_H
#define MUSIC_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 配置参数 ==================== */
#define N_SAMPLES 2048
#define WIDTH 32
#define HEIGHT 8
#define FRAME_SIZE 768 //W*H*3.  (RGB)

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化音乐可视化模块
 * - 分配/清零内部 buffer
 * - （可选）预计算窗函数、频段索引等
 */
void initMusic(void);

/**
 * @brief 处理一帧音频数据并更新内部像素帧
 * 
 * @param audiosource 输入音频，长度必须为 MUSIC_N_SAMPLES，
 *                    数据范围建议 [-1.0, 1.0]
 * 
 * @note 此函数会：
 *       1. 应用汉宁窗
 *       2. 执行 FFT
 *       3. 计算 dB 谱
 *       4. 对数分组到 32 频段
 *       5. 映射为 8 级高度
 *       6. 生成 RGB 像素帧（红柱，黑背景）
 */
void flash_audio_to_arrow(const float audiosource[N_SAMPLES]);

/**
 * @brief 获取当前生成的像素帧指针（RGB 格式）
 * 
 * @return const uint8_t* 指向内部静态 buffer，大小为 MUSIC_FRAME_SIZE
 *         格式：[R0, G0, B0, R1, G1, B1, ..., R767, G767, B767]
 *         像素排列：按列优先（i + j*WIDTH），每列从上到下
 * 
 * @warning 返回的指针指向内部静态内存，不要 free()！
 *          内容会在下次调用 flash_audio_to_ledp() 时被覆盖。
 */
const uint8_t* getMusicPointer(void);

#ifdef __cplusplus
}
#endif

#endif // MUSIC_H