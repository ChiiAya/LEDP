#ifndef DRIVER_H_
#define DRIVER_H_

#include <stdint.h>
#include "esp_err.h"

// 定义 LED 面板尺寸（可被外部使用）
#define LEDPanel_Width    32
#define LEDPanel_Height   8

// 像素结构体：每个像素包含 RGB 三色
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} ledp_pixel_t;

/**
 * @brief 初始化 RMT 外设，用于驱动 LED 面板
 * @return esp_err_t
 */
esp_err_t initRMT(void);

/**
 * @brief 将二维像素数组绘制到 LED 面板
 * 
 * 注意：面板采用 Zig-Zag（蛇形）布线：
 * - 偶数行（0,2,4...）从左到右
 * - 奇数行（1,3,5...）从右到左
 * 
 * @param pixel[LEDPanel_Height][LEDPanel_Width] 要显示的像素数据
 * @return esp_err_t
 */
esp_err_t paintLEDPanel(ledp_pixel_t pixel[LEDPanel_Height][LEDPanel_Width]);

/**
 * @brief 清空 LED 面板（全部设为黑色/关闭）
 * @return esp_err_t
 */
esp_err_t clearPanel(void);

#endif // DRIVER_H_