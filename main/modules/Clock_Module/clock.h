#ifndef CLOCK_MODULE_H
#define CLOCK_MODULE_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 时钟模块初始化（仅调用一次，不影响其他功能）
void clock_module_init(void);

#endif