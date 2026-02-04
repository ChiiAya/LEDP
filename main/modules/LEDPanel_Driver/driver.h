#ifndef DRIVER_H_
#define DRIVER_H_

#include <stdint.h>
#include "esp_err.h"

// 定义 LED 面板尺寸（可被外部使用）
#define LEDPanel_Width    32
#define LEDPanel_Height   8

/**
 * @brief 初始化 RMT 外设和显示任务，用于驱动 LED 面板
 * 
 * 此函数只需调用一次。内部会：
 * - 初始化 RMT 硬件
 * - 创建帧队列
 * - 启动后台显示任务
 * 
 * @return esp_err_t
 *         - ESP_OK: 成功
 *         - 其他: 初始化失败
 */
esp_err_t initRMT(void);

/**
 * @brief 提交一帧图像数据到 LED 面板（非阻塞）
 *
 * 输入格式：RGB 像素数组，长度 = LEDPanel_Height * LEDPanel_Width * 3
 * 像素排列顺序：行优先（row-major），即：
 *   [R₀₀, G₀₀, B₀₀, R₀₁, G₀₁, B₀₁, ..., R₀ₙ, ..., R₁₀, ...]
 *
 * 驱动内部会自动处理 Zig-Zag（蛇形）走线。
 *
 * ⚠️ 注意：此函数不会等待 LED 传输完成，立即返回。
 *         若提交过快，旧帧可能被丢弃（见日志 "Frame dropped"）。
 *
 * @param pixels 指向 RGB 像素数据的指针（只读）
 * @return esp_err_t
 *         - ESP_OK: 帧已成功入队
 *         - ESP_ERR_INVALID_STATE: 驱动未初始化
 *         - ESP_ERR_NO_MEM: 队列满，帧被丢弃
 */
esp_err_t submitLEDFrame(const uint8_t *pixels);

/**
 * @brief 清空 LED 面板（全部设为黑色/关闭）
 * 
 * 此函数会**立即发送全黑帧**并**阻塞等待完成**，
 * 适用于启动时清屏或紧急关闭。
 *
 * @return esp_err_t
 */
esp_err_t clearPanel(void);

#endif // DRIVER_H_