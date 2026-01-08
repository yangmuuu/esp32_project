#include <stdio.h>
#include "xl9555.h"
#include "esp_log.h"

#define TAG "MAIN"

void xl9555_callback(uint16_t pin, int level)
{
    switch (pin)
    {
    case IO0_1:
        ESP_LOGI(TAG, "Button1 check,level:%d", level);
        break;
    case IO0_2:
        ESP_LOGI(TAG, "Button2 check,level:%d", level);
        break;
    case IO0_3:
        ESP_LOGI(TAG, "Button3 check,level:%d", level);
        break;
    case IO0_4:
        ESP_LOGI(TAG, "Button4 check,level:%d", level);
        break;
    default:
        break;
    }
}

void app_main(void)
{
    // 初始化
    xl9555_init(GPIO_NUM_10, GPIO_NUM_11, GPIO_NUM_17, xl9555_callback);
    xl9555_ioconfig(0xFFFF);
}
