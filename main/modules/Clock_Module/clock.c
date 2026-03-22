#include "clock.h"
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

// 【关键】引入你项目实际的 LED 驱动头文件
#include "driver.h"

// 日志标签
static const char *CLOCK_TAG = "CLOCK";

// ==========================================
// 配置：时钟显示颜色
// ==========================================
#define CLOCK_COLOR_R    255 // 红色
#define CLOCK_COLOR_G    0   // 绿色
#define CLOCK_COLOR_B    0   // 蓝色

// ==========================================
// 字模：5x7 点阵数字 0-9（用于显示时间）
// ==========================================
static const uint8_t font_5x7[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}  // 9
};

// ==========================================
// 全局：帧缓冲区（用于提交给 LED 驱动）
// ==========================================
static uint8_t clock_frame[FRAME_SIZE] = {0};

// ==========================================
// 辅助函数：在帧缓冲区中设置指定坐标的像素
// ==========================================
static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || x >= LEDPanel_Width || y < 0 || y >= LEDPanel_Height) {
        return; // 越界检查
    }

    // 计算像素在数组中的索引：行优先，RGB 格式
    int index = (y * LEDPanel_Width + x) * 3;
    clock_frame[index] = r;
    clock_frame[index + 1] = g;
    clock_frame[index + 2] = b;
}

// ==========================================
// 辅助函数：清空帧缓冲区（全黑）
// ==========================================
static void clear_frame(void)
{
    memset(clock_frame, 0, sizeof(clock_frame));
}

// ==========================================
// 辅助函数：在帧缓冲区中绘制一个数字
// ==========================================
static void draw_digit(int x_offset, int digit)
{
    if (digit < 0 || digit > 9) return;

    for (int x = 0; x < 5; x++) {
        uint8_t line = font_5x7[digit][x];
        for (int y = 0; y < 7; y++) {
            if (line & (1 << y)) {
                set_pixel(x_offset + x, y, CLOCK_COLOR_R, CLOCK_COLOR_G, CLOCK_COLOR_B);
            }
        }
    }
}

// ==========================================
// 辅助函数：绘制冒号（时分秒之间的分隔符）
// ==========================================
static void draw_colon(int x_offset)
{
    // 在第2行和第4行画两个点
    set_pixel(x_offset, 2, CLOCK_COLOR_R, CLOCK_COLOR_G, CLOCK_COLOR_B);
    set_pixel(x_offset, 4, CLOCK_COLOR_R, CLOCK_COLOR_G, CLOCK_COLOR_B);
}

// ==========================================
// 时钟任务：获取时间 + 串口打印 + LED 渲染
// ==========================================
static void clock_task(void *arg)
{
    // 初始化系统时间为 00:00:00
    struct timeval tv_init = {.tv_sec = 0, .tv_usec = 0};
    settimeofday(&tv_init, NULL);

    // 设置中国标准时区 (CST-8)
    setenv("TZ", "CST-8", 1);
    tzset();

    char strftime_buf[64] = {0};
    time_t now;
    struct tm timeinfo = {0};

    ESP_LOGI(CLOCK_TAG, "时钟任务已启动，开始计时并渲染到 LED");

    while (1)
    {
        // 1. 获取当前时间
        time(&now);
        localtime_r(&now, &timeinfo);

        // 2. 串口打印时间
        strftime(strftime_buf, sizeof(strftime_buf), "%H:%M:%S", &timeinfo);
        ESP_LOGI(CLOCK_TAG, "系统时间: %s", strftime_buf);

        // ==========================================
        // 3. 【核心新增】渲染时间到 LED 面板
        // ==========================================
        // 3.1 清空帧缓冲区
        clear_frame();

        // 3.2 拆分时、分、秒为单个数字
        int h1 = timeinfo.tm_hour / 10;
        int h2 = timeinfo.tm_hour % 10;
        int m1 = timeinfo.tm_min / 10;
        int m2 = timeinfo.tm_min % 10;
        int s1 = timeinfo.tm_sec / 10;
        int s2 = timeinfo.tm_sec % 10;

        // 3.3 绘制时间（布局：HH:MM:SS，适配 32x8 面板）
        draw_digit(0,  h1); // 时 - 十位
        draw_digit(6,  h2); // 时 - 个位
        draw_colon(11);      // 冒号
        draw_digit(13, m1); // 分 - 十位
        draw_digit(19, m2); // 分 - 个位
        draw_colon(24);      // 冒号
        draw_digit(26, s1); // 秒 - 十位
        draw_digit(32, s2); // 秒 - 个位

        // 3.4 【关键】提交帧缓冲区给 LED 驱动显示
        submitLEDFrame(clock_frame);

        // 4. 延时 1 秒
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ==========================================
// 时钟初始化接口（保持不变，不影响其他代码）
// ==========================================
void clock_module_init(void)
{
    // 调大栈到 8192，确保 LED 渲染有足够栈空间
    xTaskCreatePinnedToCore(
        clock_task,
        "clock_task",
        8192,
        NULL,
        1,
        NULL,
        1
    );
    ESP_LOGI(CLOCK_TAG, "时钟模块初始化成功（含 LED 渲染）");
}