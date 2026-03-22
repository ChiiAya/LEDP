#include "clock.h"
#include "esp_log.h"
#include <time.h>

// 日志标签（独立，不与原有代码冲突）
static const char *CLOCK_TAG = "CLOCK_MODULE";

// 独立时钟任务：与原有LED任务并行运行，互不干扰
static void clock_task(void *arg)
{
    // 初始化系统时间（从 00:00:00 开始计时）
    struct tm timeinfo = {0};
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;

    while (1)
    {
        // 每秒更新一次时间
        timeinfo.tm_sec++;
        if (timeinfo.tm_sec >= 60) {
            timeinfo.tm_sec = 0;
            timeinfo.tm_min++;
        }
        if (timeinfo.tm_min >= 60) {
            timeinfo.tm_min = 0;
            timeinfo.tm_hour++;
        }
        if (timeinfo.tm_hour >= 24) {
            timeinfo.tm_hour = 0;
        }

        // 串口打印时钟（可屏蔽，不影响LED）
        ESP_LOGI(CLOCK_TAG, "系统时间: %02d:%02d:%02d",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        // 延时1秒，精准计时
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// 时钟初始化：创建独立任务，不修改原有任何逻辑
void clock_module_init(void)
{
    xTaskCreate(
        clock_task,   // 任务函数
        "clock_task", // 任务名称
        2048,         // 栈大小
        NULL,         // 参数
        1,            // 优先级
        NULL          // 任务句柄
    );
    ESP_LOGI(CLOCK_TAG, "独立时钟模块初始化完成");
}