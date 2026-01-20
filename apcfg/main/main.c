#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include "esp_spiffs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <string.h>
#include "ap_wifi.h"
#include "button.h"
#include "xl9555.h"

#define TAG     "main"

#define XL9555_SDA  GPIO_NUM_10
#define XL9555_SCL  GPIO_NUM_11

//按位表示
static volatile uint16_t xl9555_button_level = 0xFFFF;

int get_button_level(int gpio)
{
    return (xl9555_button_level&gpio)?1:0;
}

void xl9555_input_callback(uint16_t io_num,int level)
{
    if(level)
        xl9555_button_level |= io_num;
    else
        xl9555_button_level &= ~io_num;
}

void long_press(int gpio)
{
    ap_wifi_apcfg(true);
}

void button_init(void)
{
    button_config_t button_cfg = 
    {
        .active_level = 0,
        .getlevel_cb = get_button_level,
        .gpio_num = IO0_1,
        .long_cb = long_press,
        .long_press_time = 3000,
        .short_cb = NULL,
    };
    button_event_set(&button_cfg);
}

void wifi_state_handle(WIFI_STATE state)
{
    if(state == WIFI_STATE_CONNECTED)
    {
        ESP_LOGI(TAG,"Wifi connected");
    }
    else if(state == WIFI_STATE_CONNECTED)
    {
        ESP_LOGI(TAG,"Wifi disconnected");
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    xl9555_init(XL9555_SDA,XL9555_SCL,GPIO_NUM_17,xl9555_input_callback);
    xl9555_ioconfig(0xFFFF);
    button_init();
    ap_wifi_init(wifi_state_handle);
}
