#include "button.h"
#include "esp_timer.h"
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#define TAG "button"

static button_info_t *button_head = NULL; // 按键头指针

static void button_handle(void *arg);
static esp_timer_handle_t button_timer_handle;
static bool timer_running = false;

esp_err_t button_event_set(button_config_t *cfg)
{
    button_info_t *btn = (button_info_t *)malloc(sizeof(button_info_t));
    if (!btn)
    {
        return ESP_FAIL;
    }
    memset(btn, 0, sizeof(button_info_t));
    // 内存拷贝
    memcpy(&btn->btn_cfg, cfg, sizeof(button_config_t));

    if (!button_head)
    {
        button_head = btn;
    }
    else
    {
        button_info_t *info = button_head;
        while (info->next)
        {
            info = info->next;
        }
        info->next = btn;
    }

    if (!timer_running)
    { // 定时器间隔
        static int button_interval = 5;
        esp_timer_create_args_t button_timer = {
            .callback = button_handle,
            .name = "button_timer",
            .dispatch_method = ESP_TIMER_TASK,
            .arg = (void *)button_interval,
        };

        // 创建定时器 返回句柄
        esp_timer_create(&button_timer, &button_timer_handle);
        esp_timer_start_periodic(button_timer_handle, 5000); // 5000us 一次}
        timer_running = true;
    }
    return ESP_OK;
}
// 按钮回调函数
static void button_handle(void *arg)
{
    button_info_t *btn_info = button_head;
    int interval = (int)arg;
    for (; btn_info; btn_info = btn_info->next)
    {
        int gpio_num = btn_info->btn_cfg.gpio_num;
        switch (btn_info->state)
        {
        case BUTTON_RELEASE: // 按键松开 空闲状态
            // 是否被按下，判断是否是按下的电平
            if (btn_info->btn_cfg.getlevel_cb(gpio_num) == btn_info->btn_cfg.active_level)
            {
                // 有按钮按下，检测到了
                btn_info->state = BUTTON_PRESS;
                btn_info->press_cnt += interval;
            }
            break;
        case BUTTON_PRESS: // 按键按下 消抖
            // 判断按键有没有松开
            if (btn_info->btn_cfg.getlevel_cb(gpio_num) == btn_info->btn_cfg.active_level)
            {
                btn_info->press_cnt += interval;
                if (btn_info->press_cnt >= 20)
                {
                    if (btn_info->btn_cfg.short_press_cb)
                    {
                        btn_info->btn_cfg.short_press_cb(gpio_num);
                        btn_info->state = BUTTON_HOLD; // 状态改为长按
                    }
                }
            }
            else
            {
                btn_info->state = BUTTON_RELEASE;
                btn_info->press_cnt = 0;
            }
            break;
        case BUTTON_HOLD: // 按键按住状态
            if (btn_info->btn_cfg.getlevel_cb(gpio_num) == btn_info->btn_cfg.active_level)
            {
                btn_info->press_cnt += interval;
                if (btn_info->press_cnt >= btn_info->btn_cfg.long_press_time)
                {
                    if (btn_info->btn_cfg.long_press_cb)
                    {                   
                        btn_info->btn_cfg.long_press_cb(gpio_num);
                        btn_info->state = BUTTON_LONG_PRESS_HOLD; 
                    }
                }
            }
            else
            {
                btn_info->state = BUTTON_RELEASE;
                btn_info->press_cnt = 0;
            }
            break;
        case BUTTON_LONG_PRESS_HOLD: // 等待松手
            if (btn_info->btn_cfg.getlevel_cb(gpio_num) != btn_info->btn_cfg.active_level)
            {
                btn_info->state = BUTTON_RELEASE;
                btn_info->press_cnt = 0;
            }
            break;

        default:
            break;
        }
    }
}
