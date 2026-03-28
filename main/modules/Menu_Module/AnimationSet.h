#include <driver.h>
#include <esp_timer.h>
#include <semaphore.h>
#include <menu.h>

typedef struct {
    uint8_t *framebuffer;
    uint8_t *anim_buffer;
    uint8_t x_startpoint;
    uint8_t y_startpoint;
    uint8_t direction;
    uint8_t current_step;
    uint8_t total_steps;
    esp_timer_handle_t timer_handle;
} FadeAnimationState_t;

// 定时器回调函数
static void fade_timer_callback(void *arg) {
    FadeAnimationState_t *state = (FadeAnimationState_t *)arg;
    
    if (state->current_step >= state->total_steps) {
        // 动画完成，停止定时器并释放资源
        esp_timer_stop(state->timer_handle);
        esp_timer_delete(state->timer_handle);
        free(state->anim_buffer);
        free(state);
        //通知给menu 
        if(xSemaAnimationOver != NULL){
            xSemaphoreGive(xSemaAnimationOver);
        }
        return;
    }
    
    // 计算当前透明度比例 (0-255)
    uint8_t alpha = 255 - (state->current_step * 255 / state->total_steps);
    
    // 执行淡出绘制逻辑
    switch (state->direction) {//现在启动点这个参数是无用的，没有实现
        case 0:
            // LEFT
            uint8_t offset = state->current_step * LEDPanel_Width/ state->total_steps;
            for (int i = LEDPanel_Width - 1; i >=0 ; i--) {
                for (int j = 0; j < LEDPanel_Height; j++) {
                    int src_idx = (i * LEDPanel_Height + j) * 3;
                    int dst_i = i - offset;  // 目标列 = 源列 - 偏移量
                    
                    if (dst_i >= 0) {
                        // 在边界内：移动并淡化
                        int dst_idx = (dst_i * LEDPanel_Height + j) * 3;
                        state->anim_buffer[dst_idx + 0] = (uint8_t)(state->framebuffer[src_idx + 0] * alpha / 255);
                        state->anim_buffer[dst_idx + 1] = (uint8_t)(state->framebuffer[src_idx + 1] * alpha / 255);
                        state->anim_buffer[dst_idx + 2] = (uint8_t)(state->framebuffer[src_idx + 2] * alpha / 255);
                    }
                    // dst_i < 0 的部分自然丢弃（滑出边界）
                }
            }
            // 清空右侧空白区域（被滑出的部分）
            for (int i = LEDPanel_Width - offset; i < LEDPanel_Width; i++) {
                for (int j = 0; j < LEDPanel_Height; j++) {
                    int idx = (j * LEDPanel_Width + i) * 3;
                    state->anim_buffer[idx + 0] = 0;
                    state->anim_buffer[idx + 1] = 0;
                    state->anim_buffer[idx + 2] = 0;
                }
            }
            break;
        case 1: 
            // RIGHT
            uint8_t offsetR = state->current_step * LEDPanel_Width / state->total_steps;
            // 修复：从左到右遍历列，避免覆盖
            for (int i = 0; i < LEDPanel_Width; i++) {
                for (int j = 0; j < LEDPanel_Height; j++) {
                    int src_idx = (j * LEDPanel_Width + i) * 3;
                    int dst_i = i + offsetR;  // 目标列
                    if (dst_i < LEDPanel_Width) {
                        int dst_idx = (j * LEDPanel_Width + dst_i) * 3;
                        state->anim_buffer[dst_idx + 0] = (uint8_t)(state->framebuffer[src_idx + 0] * alpha / 255);
                        state->anim_buffer[dst_idx + 1] = (uint8_t)(state->framebuffer[src_idx + 1] * alpha / 255);
                        state->anim_buffer[dst_idx + 2] = (uint8_t)(state->framebuffer[src_idx + 2] * alpha / 255);
                    }
                }
            }
            // 清空左侧空白区域
            for (int i = 0; i < offsetR; i++) {
                for (int j = 0; j < LEDPanel_Height; j++) {
                    int idx = (j * LEDPanel_Width + i) * 3;
                    state->anim_buffer[idx + 0] = 0;
                    state->anim_buffer[idx + 1] = 0;
                    state->anim_buffer[idx + 2] = 0;
                }
            }
            break;
        case 2:
            // UP
            ESP_LOGW("AnimationSet","This animation is not implemented");
            break;
        case 3:
            // DOWN
            ESP_LOGW("AnimationSet","This animation is not implemented");
            break;
    }
    submitLEDFrame(state->anim_buffer);
    state->current_step++;
}

// 淡出动画主函数
void fade(uint8_t *framebuffer,
          uint8_t x_startpoint, 
          uint8_t y_startpoint,
          uint8_t direction,//0 -> LEFT | 1 -> RIGHT | 2 -> UP | 3 -> DOWN
          float duration  // 单位：秒
) {
    
    
    // 创建独立的动画缓冲区
    uint8_t *anim_buffer = (uint8_t *)malloc(FRAME_SIZE);
    if (!anim_buffer) return;
    // 复制原始帧到动画缓冲区
    memcpy(anim_buffer, framebuffer, FRAME_SIZE);
    // 创建动画状态
    FadeAnimationState_t *state = (FadeAnimationState_t *)malloc(sizeof(FadeAnimationState_t));
    if (state == NULL){
        free(anim_buffer);
        free(state);
        return;
    };
    
    state->framebuffer = framebuffer;
    state->anim_buffer = anim_buffer;
    state->x_startpoint = x_startpoint;
    state->y_startpoint = y_startpoint;
    state->direction = direction;
    state->current_step = 0;
    state->total_steps = 64;  // 64 级渐变
    
    // 创建定时器
    esp_timer_create_args_t timer_args = {
        .callback = fade_timer_callback,
        .arg = state,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "fade_timer"
    };
    esp_timer_handle_t local_timer_handle;
    if (esp_timer_create(&timer_args, &local_timer_handle) != ESP_OK) {
        free(state);
        return;
    }
    state->timer_handle = local_timer_handle;
    // 计算每次回调间隔 (总时间/步数)
    uint32_t interval_us = (duration * 1000000) / state->total_steps;
    
    // 启动周期定时器
    esp_timer_start_periodic(local_timer_handle, interval_us);
}