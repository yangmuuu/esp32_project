#ifndef _BUTTON_H_
#define _BUTTON_H_
#include "esp_err.h"
// 按键回调函数
typedef void(*button_press_cb_t)(int gpio_num);

// 获取GPIO电平的操作回调函数
typedef int(*button_getlevel_cb_t)(int gpio_num);

// 按键配置结构体
typedef struct 
{
    int gpio_num; // gpio口
    int active_level;  // 按下的电平
    int long_press_time; // 长按时间
    button_press_cb_t short_press_cb; // 按键短按回调
    button_press_cb_t long_press_cb; // 按键长按回调
    button_getlevel_cb_t getlevel_cb; // 获取GPIO电平的回调
}button_config_t;

// btn状态
typedef enum
{
    BUTTON_RELEASE, // 按键松开
    BUTTON_PRESS, // 按键按下 消抖
    BUTTON_HOLD, // 按键按住状态
    BUTTON_LONG_PRESS_HOLD, // 等待松手
}BUTTON_STATE;

// 按键状态信息
typedef struct Button_info
{
    button_config_t btn_cfg; // 按键配置
    BUTTON_STATE state; // 按键状态
    int press_cnt; // 按下时间
    struct Button_info *next; // 下一个按键信息
}button_info_t;


esp_err_t button_event_set(button_config_t* cfg);

#endif