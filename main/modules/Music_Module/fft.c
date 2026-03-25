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
 #define ADC_factor 0.05f   // 降低自适应速度，原本 0.9f 过快导致跳变 
 #define TARGET_FPS 120                          // 目标帧率 
 #define MIN_INTERVAL_US (1000000LL / TARGET_FPS) // T 

 // 优化平滑参数：快升慢降 
 #define SMOOTH_UP   0.8f   // 上升系数 (0-1)，越大越灵敏 
 #define SMOOTH_DOWN 0.15f  // 下降系数 (0-1)，越小越平滑，解决“响应剧烈”的关键 
 
 static int64_t last_frame_time = 0;  // 上一帧时间（微秒） 
 static uint32_t frame_count = 0;     // 帧计数 
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
 static uint8_t ColumnHeight[LEDPanel_Width] = {0}; 
 static float ColumnHeight_f[LEDPanel_Width] = {0}; // 新增：记录浮点高度以实现平滑 
 static float Pre_avr_db = 0.0; 
 
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
     if (!audiosource) return; 
     
     for (int i = 0 ; i < N ; i++) { 
         y_cf[i * 2 + 0] = audiosource[i] * wind[i]; 
         y_cf[i * 2 + 1] = 0; 
     } 
     dsps_fft2r_fc32(y_cf, N); 
     dsps_bit_rev_fc32(y_cf, N); 
 
     for (int i = 0 ; i < N / 2 ; i++) { 
         sum_y[i] = 10 * log10f((y_cf[i * 2 + 0] * y_cf[i * 2 + 0] + y_cf[i * 2 + 1] * y_cf[i * 2 + 1]) / N); 
     } 
 
     // 动态增益计算平均功率 
     float db_avr = 0.0; 
     for(int i = 0;i < N/2;i++){ 
         db_avr += sum_y[i]; 
     } 
     db_avr /= N/2; 
     Pre_avr_db = ADC_factor * db_avr + (1 - ADC_factor) * Pre_avr_db; 
     
     float min_db = MINDB; 
     float max_db = Pre_avr_db + 20.0f; 
 
     if(max_db > MAXDB_MAX) max_db = MAXDB_MAX; 
     if(max_db < MAXDB_MIN) max_db = MAXDB_MIN; 
 
     // 归一化 
     for(int i = 0;i < N/2;i++){ 
         float clamped_db = fmaxf(sum_y[i], min_db); 
         clamped_db = fminf(clamped_db, max_db); 
         sum_y[i] = (clamped_db - min_db) / (max_db - min_db); 
     } 
 
     // 决定高度（加入时间平滑逻辑） 
     for(int i = 0; i < LEDPanel_Width; i++){ 
         temp = 0.0; 
         int start = fft_index[i];
         int end = fft_index[i+1];
         if (end > start) {
             for(int j = start; j < end; j++){ 
                 temp += sum_y[j]; 
             } 
             temp = temp / (end - start); 
         }

         // 计算目标高度（映射到面板高度）
         float target_height = temp * LEDPanel_Height; 

         // 快升慢降平滑算法
         if (target_height > ColumnHeight_f[i]) {
             // 上升阶段：使用 SMOOTH_UP，保持打击感
             ColumnHeight_f[i] = SMOOTH_UP * target_height + (1.0f - SMOOTH_UP) * ColumnHeight_f[i];
         } else {
             // 下降阶段：使用 SMOOTH_DOWN，让回落变丝滑
             ColumnHeight_f[i] = SMOOTH_DOWN * target_height + (1.0f - SMOOTH_DOWN) * ColumnHeight_f[i];
         }

         // 更新最终显示的整数高度
         ColumnHeight[i] = (uint8_t)ColumnHeight_f[i];
         if (ColumnHeight[i] >= LEDPanel_Height) ColumnHeight[i] = LEDPanel_Height - 1;
     } 
 
     // 衰减背景帧
     for(int i = 0; i < LEDPanel_Width * LEDPanel_Height * 3; i++){ 
         s_pixel_frame_f[i] *= decay_factor; 
     } 
 
     // 绘制
     for(int i = 0; i < LEDPanel_Width; i++){ 
         uint32_t hue = i * (300 / (LEDPanel_Width - 1)); 
         uint32_t value = 20; 
         uint32_t saturation = 100; 
         uint32_t r, g, b; 
         float scale = 64.0f / 255.0f; 
         led_strip_hsv2rgb(hue, saturation, value, &r, &g, &b); 
 
         for(int j = 0; j < LEDPanel_Height; j++){ 
             if(ColumnHeight[i] >= j){ 
                 s_pixel_frame_f[(i + j * LEDPanel_Width) * 3 + 0] = r * scale; 
                 s_pixel_frame_f[(i + j * LEDPanel_Width) * 3 + 1] = g * scale; 
                 s_pixel_frame_f[(i + j * LEDPanel_Width) * 3 + 2] = b * scale; 
             } 
         } 
     } 

     // 转换为 uint8
     for(int i = 0; i < LEDPanel_Width * LEDPanel_Height * 3; i++){ 
         s_pixel_frame[i] = (uint8_t)s_pixel_frame_f[i]; 
     } 

     // 提交
     int64_t now_time = esp_timer_get_time(); 
     if (now_time - last_frame_time >= MIN_INTERVAL_US) { 
         submitLEDFrame(s_pixel_frame); 
         
         // FPS 统计
         if (last_frame_time != 0) {
             float instant_fps = 1000000.0f / (now_time - last_frame_time);
             avg_fps = 0.9f * avg_fps + 0.1f * instant_fps;
             if (++frame_count >= 120) {
                 ESP_LOGI(TAG, "Avg FPS: %.1f", avg_fps);
                 frame_count = 0;
             }
         }
         last_frame_time = now_time; 
     } 
     esp_task_wdt_reset(); 
 }
