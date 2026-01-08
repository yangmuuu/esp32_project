#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"

void led_flash_init(void)
{
    // gpio 配置结构体
    gpio_config_t led_cfg =
        {
            // 位掩码
            .pin_bit_mask = (1 << GPIO_NUM_15),
            // 模式
            .mode = GPIO_MODE_OUTPUT,
        };
    gpio_config(&led_cfg);
}

// 初始化呼吸灯
void led_breath_init(void)
{
    // 初始化定时器
    ledc_timer_config_t ledc_timer =
        {
            .clk_cfg = LEDC_AUTO_CLK, // 根据周期和频率选择一个最合适的时钟源
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = LEDC_TIMER_0,            // 定时器编号
            .duty_resolution = LEDC_TIMER_12_BIT, // 占空比分辨率 定时器最大计数值
            .freq_hz = 5000,                      // pwm 频率 就是等于定时器频率
        };
    ledc_timer_config(&ledc_timer);
    // 初始化PWM通道
    ledc_channel_config_t ledc_channel =
        {
            .channel = LEDC_CHANNEL_0,
            .duty = 0, // 占空比
            .gpio_num = GPIO_NUM_15,
            .speed_mode = LEDC_LOW_SPEED_MODE, // 低速模式
            .timer_sel = LEDC_TIMER_0,         // 选择定时器0
        };
    ledc_channel_config(&ledc_channel);
    // 渐变
    ledc_fade_func_install(0); // 启动硬件渐变功能
}

void app_main(void)
{
    led_breath_init();
    while (1)
    {
        // 4095: 渐变结束时的亮度  目标占空比
        // 专门用于生成 PWM 信号 配置渐变参数
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4095, 2000);
        // 启动渐变 第三个参数 是否阻塞
        ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_WAIT_DONE);

        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0, 2000);
        // 启动渐变 第三个参数 是否阻塞
        ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_WAIT_DONE);
    }
}
