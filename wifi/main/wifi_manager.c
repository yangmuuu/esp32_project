#include "wifi_manager.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <stdio.h>
#include <string.h>

#define MAX_CONNECT_RETRY 10
#define TAG "wifi_manager"

static int sta_connect_cnt = 0; // 连接次数

// wifi状态回调函数
static p_wifi_state_cb wifi_callback = NULL;

// 标记当前是否连接到路由器,当前sta连接状态
static bool is_sta_connected = false;

// 事件回调函数
static void event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) // wifi事件
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START: // wifi启动
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED: // wifi断开连接
            if (is_sta_connected)
            {
                is_sta_connected = false; // 断开连接标志
                if (wifi_callback)
                {
                    wifi_callback(WIFI_STATE_DISCONNECTED);
                }
            }
            if (sta_connect_cnt < MAX_CONNECT_RETRY)
            {
                esp_wifi_connect(); // 失败重连
                sta_connect_cnt++;
            }
            break;
        case WIFI_EVENT_STA_CONNECTED: // wifi连接成功
            ESP_LOGI(TAG, "Connected to ap");
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT) // ip事件
    {
        // 获取IP并打印
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            ESP_LOGI(TAG, "got ip");
            is_sta_connected = true; // 已经连接
            if (wifi_callback != NULL)
            {
                wifi_callback(WIFI_STATE_CONNECTED);
            }
        }
    }
}
void wifi_manager_init(p_wifi_state_cb f)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    wifi_callback = f;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_manager_connect(const char *ssid, const char *password)
{
    wifi_config_t wifi_config = {
        .sta =
            {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
    };
    snprintf((char *)wifi_config.sta.ssid, 31, "%s", ssid);
    snprintf((char *)wifi_config.sta.password, 63, "%s", password);
    // 判断当前是不是在sta模式下
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode != WIFI_MODE_STA)
    {
        // 切换一下
        esp_wifi_stop(); // 停止wifi工作模式
        esp_wifi_set_mode(WIFI_MODE_STA);
    }
    sta_connect_cnt = 0; // 重置连接次数
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_start();
}