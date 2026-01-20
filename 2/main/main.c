#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"

#include "usb_stream.h"
#include "button.h"
#include "xl9555.h"
//#include "ap_wifi.h"

#define TAG "main"

/* ===================== XL9555 ===================== */
#define XL9555_SDA  GPIO_NUM_10
#define XL9555_SCL  GPIO_NUM_11

/* ===================== UVC 参数（保守稳定版） ===================== */
/* 淘宝说明明确支持 */
#define UVC_FRAME_WIDTH     320
#define UVC_FRAME_HEIGHT    240
#define UVC_FRAME_INTERVAL  FPS2INTERVAL(25)   // 15 FPS（比 30 稳定）

/* 关键：USB Host DMA 安全值 */
#define UVC_XFER_BUFFER_SIZE   (4 * 1024)     // 16KB，别贪
#define UVC_FRAME_BUFFER_SIZE  (64 * 1024)     // MJPEG 一帧最大

static uint8_t *xfer_buffer_a = NULL;
static uint8_t *xfer_buffer_b = NULL;
static uint8_t *frame_buffer  = NULL;

static bool s_camera_connected = false;

/* ===================== 摄像头帧回调 ===================== */
static void camera_frame_cb(uvc_frame_t *frame, void *ptr)
{
    ESP_LOGI(TAG, "Frame: %dx%d size=%d",
             frame->width,
             frame->height,
             frame->data_bytes);

    /* frame->data 是 MJPEG，可直接存或送给解码器 */
}

/* ===================== XL9555 按键 ===================== */
static volatile uint16_t xl9555_button_level = 0xFFFF;

int get_button_level(int gpio)
{
    return (xl9555_button_level & gpio) ? 1 : 0;
}

void xl9555_input_callback(uint16_t io_num, int level)
{
    if (level)
        xl9555_button_level |= io_num;
    else
        xl9555_button_level &= ~io_num;
}

void long_press(int gpio)
{
    // ap_wifi_apcfg(true);
}

void short_press(int gpio)
{
    ESP_LOGI(TAG, "Button short press");
    if (s_camera_connected) {
        ESP_LOGI(TAG, "Camera READY");
    } else {
        ESP_LOGW(TAG, "Camera NOT ready");
    }
}

void button_init(void)
{
    button_config_t cfg1 = {
        .gpio_num = IO0_1,
        .active_level = 0,
        .getlevel_cb = get_button_level,
        .long_press_time = 3000,
        .long_cb = long_press,
    };
    button_event_set(&cfg1);

    button_config_t cfg2 = {
        .gpio_num = IO0_2,
        .active_level = 0,
        .getlevel_cb = get_button_level,
        .short_cb = short_press,
    };
    button_event_set(&cfg2);
}

/* ===================== USB 流状态回调 ===================== */
static void stream_state_changed_cb(usb_stream_state_t event, void *arg)
{
    if (event == STREAM_CONNECTED) {
        ESP_LOGI(TAG, "USB Camera connected");
        s_camera_connected = true;
    } else if (event == STREAM_DISCONNECTED) {
        ESP_LOGW(TAG, "USB Camera disconnected");
        s_camera_connected = false;
    }
}

/* ===================== 主函数 ===================== */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    xl9555_init(XL9555_SDA, XL9555_SCL, GPIO_NUM_17, xl9555_input_callback);
    xl9555_ioconfig(0xFFFF);
    button_init();

    /* ========== 关键：USB DMA buffer 必须 INTERNAL ========== */
    xfer_buffer_a = heap_caps_malloc(
        UVC_XFER_BUFFER_SIZE,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA
    );
    xfer_buffer_b = heap_caps_malloc(
        UVC_XFER_BUFFER_SIZE,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA
    );

    /* 帧数据可以放 PSRAM */
    frame_buffer = heap_caps_malloc(
        UVC_FRAME_BUFFER_SIZE,
        MALLOC_CAP_SPIRAM
    );

    if (!xfer_buffer_a || !xfer_buffer_b || !frame_buffer) {
        ESP_LOGE(TAG, "Buffer alloc failed");
        return;
    }

    /* ========== UVC 配置（稳定版） ========== */
    uvc_config_t uvc_config = {
        .frame_width        = UVC_FRAME_WIDTH,
        .frame_height       = UVC_FRAME_HEIGHT,
        .frame_interval     = UVC_FRAME_INTERVAL,

        .xfer_buffer_size   = UVC_XFER_BUFFER_SIZE,
        .xfer_buffer_a      = xfer_buffer_a,
        .xfer_buffer_b      = xfer_buffer_b,

        .frame_buffer_size  = UVC_FRAME_BUFFER_SIZE,
        .frame_buffer       = frame_buffer,

        .frame_cb           = camera_frame_cb,
        .frame_cb_arg       = NULL,
    };

    ESP_ERROR_CHECK(usb_streaming_state_register(
        stream_state_changed_cb, NULL
    ));

    ESP_ERROR_CHECK(uvc_streaming_config(&uvc_config));

    esp_err_t ret = usb_streaming_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "usb_streaming_start failed: %s",
                 esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "USB streaming started");
    }
}
