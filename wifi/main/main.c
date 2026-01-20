#include <stdio.h>
#include "wifi_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "main"

#define SSID "testwifi"
#define PASSWORD "12345678"

void wifi_state_handler(WIFI_STATE state)
{
    if (state == WIFI_STATE_CONNECTED)  
    {
        ESP_LOGI(TAG, "WIFI CONNECTED");
    }
    if(state == WIFI_STATE_DISCONNECTED)
    {
        ESP_LOGI(TAG, "WIFI DISCONNECTED");
    }
    
}

void app_main(void)
{
    nvs_flash_init(); // init nvs

    wifi_manager_init(wifi_state_handler);
    
    wifi_manager_connect(SSID, PASSWORD);

    while (1)
    {
        vTaskDelay(20);
    }
    
}
