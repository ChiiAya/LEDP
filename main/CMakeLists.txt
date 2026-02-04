idf_component_register(
SRCS    
    "led_strip_example_main.c" 
    "modules/LEDPanel_Driver/led_strip_encoder.c" 
    "modules/LEDPanel_Driver/driver.c"
    "modules/Clock_Module/clock.c"
    "modules/Music_Module/fft.c"
    "modules/Music_Module/audio.c"

PRIV_REQUIRES 
    esp_driver_rmt 
    esp_driver_spi
    esp_driver_gpio
    esp_driver_uart
    esp_driver_i2s
INCLUDE_DIRS 
    "." "modules/Clock_Module" "modules/LEDPanel_Driver" "modules/Music_Module")
