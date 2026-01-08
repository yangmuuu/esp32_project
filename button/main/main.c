#include <stdio.h>
#include "button.h"
#include "xl9555.h"

void xl9555_input_callback(uint16_t io_num, int level)
{

}

void app_main(void)
{
    // 初始化 xl9555
    xl9555_init(GPIO_NUM_10, GPIO_NUM_11, GPIO_NUM_17, xl9555_input_callback);

    // 配置按键
    button_config_t btn_cfg = {
        .active_level = 0, // 按键按下为低电平， 默认上拉
        .gpio_num = IO0_0,
    };
    // 所有的gpio口为输入
    xl9555_ioconfig(0xFFFF);
}
