/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_stream.h"

/*
    官方USB摄像头例程，删减了部分无用代码，后续加入注释
    测试需要购买乐鑫官方的USB摄像头，在上电后，连接热点ESP32S3-UVC
    在浏览器中输入192.168.4.1即可开启监控
*/



static const char *TAG = "uvc_mic_spk_demo";
/****************** configure the example working mode *******************************/
#define ENABLE_UVC_CAMERA_FUNCTION        1        /* enable uvc function */
#define ENABLE_UAC_MIC_SPK_FUNCTION       0        /* enable uac mic+spk function */
#if (ENABLE_UVC_CAMERA_FUNCTION)
#define ENABLE_UVC_FRAME_RESOLUTION_ANY   1        /* Using any resolution found from the camera */
#define ENABLE_UVC_WIFI_XFER              1        /* transfer uvc frame to wifi http */
#endif

#define BIT0_FRAME_START     (0x01 << 0)
#define BIT1_NEW_FRAME_START (0x01 << 1)
#define BIT2_NEW_FRAME_END   (0x01 << 2)
#define BIT3_SPK_START       (0x01 << 3)
#define BIT4_SPK_RESET       (0x01 << 4)

static EventGroupHandle_t s_evt_handle;

#define DEMO_UVC_FRAME_WIDTH        FRAME_RESOLUTION_ANY
#define DEMO_UVC_FRAME_HEIGHT       FRAME_RESOLUTION_ANY

#define DEMO_UVC_XFER_BUFFER_SIZE (55 * 1024)

#include "app_wifi.h"
#include "app_httpd.h"
#include "esp_camera.h"

static camera_fb_t s_fb = {0};

camera_fb_t *esp_camera_fb_get()
{
    xEventGroupSetBits(s_evt_handle, BIT0_FRAME_START);
    xEventGroupWaitBits(s_evt_handle, BIT1_NEW_FRAME_START, true, true, portMAX_DELAY);
    return &s_fb;
}

void esp_camera_fb_return(camera_fb_t *fb)
{
    xEventGroupSetBits(s_evt_handle, BIT2_NEW_FRAME_END);
    return;
}

static void camera_frame_cb(uvc_frame_t *frame, void *ptr)
{
    ESP_LOGI(TAG, "uvc callback! frame_format = %d, seq = %"PRIu32", width = %"PRIu32", height = %"PRIu32", length = %u, ptr = %d",
             frame->frame_format, frame->sequence, frame->width, frame->height, frame->data_bytes, (int) ptr);
    if (!(xEventGroupGetBits(s_evt_handle) & BIT0_FRAME_START)) {
        return;
    }

    switch (frame->frame_format) {
    case UVC_FRAME_FORMAT_MJPEG:
        s_fb.buf = frame->data;
        s_fb.len = frame->data_bytes;
        s_fb.width = frame->width;
        s_fb.height = frame->height;
        s_fb.buf = frame->data;
        s_fb.format = PIXFORMAT_JPEG;
        s_fb.timestamp.tv_sec = frame->sequence;
        xEventGroupSetBits(s_evt_handle, BIT1_NEW_FRAME_START);
        ESP_LOGV(TAG, "send frame = %"PRIu32"", frame->sequence);
        xEventGroupWaitBits(s_evt_handle, BIT2_NEW_FRAME_END, true, true, portMAX_DELAY);
        ESP_LOGV(TAG, "send frame done = %"PRIu32"", frame->sequence);
        break;
    default:
        ESP_LOGW(TAG, "Format not supported");
        assert(0);
        break;
    }
}



#if (ENABLE_UAC_MIC_SPK_FUNCTION)
static void mic_frame_cb(mic_frame_t *frame, void *ptr)
{
    // We should using higher baudrate here, to reduce the blocking time here
    ESP_LOGD(TAG, "mic callback! bit_resolution = %u, samples_frequence = %"PRIu32", data_bytes = %"PRIu32,
             frame->bit_resolution, frame->samples_frequence, frame->data_bytes);
    // We should never block in mic callback!
#if (ENABLE_UAC_MIC_SPK_LOOPBACK)
    uac_spk_streaming_write(frame->data, frame->data_bytes, 0);
#endif //ENABLE_UAC_MIC_SPK_LOOPBACK
}
#endif //ENABLE_UAC_MIC_SPK_FUNCTION

static void stream_state_changed_cb(usb_stream_state_t event, void *arg)
{
    switch (event) {
    case STREAM_CONNECTED: {
        size_t frame_size = 0;
        size_t frame_index = 0;
#if (ENABLE_UVC_CAMERA_FUNCTION)
        uvc_frame_size_list_get(NULL, &frame_size, &frame_index);
        if (frame_size) {
            ESP_LOGI(TAG, "UVC: get frame list size = %u, current = %u", frame_size, frame_index);
            uvc_frame_size_t *uvc_frame_list = (uvc_frame_size_t *)malloc(frame_size * sizeof(uvc_frame_size_t));
            uvc_frame_size_list_get(uvc_frame_list, NULL, NULL);
            for (size_t i = 0; i < frame_size; i++) {
                ESP_LOGI(TAG, "\tframe[%u] = %ux%u", i, uvc_frame_list[i].width, uvc_frame_list[i].height);
            }
            free(uvc_frame_list);
        } else {
            ESP_LOGW(TAG, "UVC: get frame list size = %u", frame_size);
        }
#endif
        ESP_LOGI(TAG, "Device connected");
        break;
    }
    case STREAM_DISCONNECTED:
        ESP_LOGI(TAG, "Device disconnected");
        break;
    default:
        ESP_LOGE(TAG, "Unknown event");
        break;
    }
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("httpd_txrx", ESP_LOG_INFO);
    esp_err_t ret = ESP_FAIL;
    s_evt_handle = xEventGroupCreate();
    if (s_evt_handle == NULL) {
        ESP_LOGE(TAG, "line-%u event group create failed", __LINE__);
        assert(0);
    }
    app_wifi_main();
    app_httpd_main();
    /* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
    uint8_t *xfer_buffer_a = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(xfer_buffer_a != NULL);
    uint8_t *xfer_buffer_b = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(xfer_buffer_b != NULL);

    /* malloc frame buffer for a jpeg frame*/
    uint8_t *frame_buffer = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(frame_buffer != NULL);

    uvc_config_t uvc_config = {
        /* match the any resolution of current camera (first frame size as default) */
        .frame_width = DEMO_UVC_FRAME_WIDTH,
        .frame_height = DEMO_UVC_FRAME_HEIGHT,
        .frame_interval = FPS2INTERVAL(15),
        .xfer_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
        .frame_buffer = frame_buffer,
        .frame_cb = &camera_frame_cb,
        .frame_cb_arg = NULL,
    };
    /* config to enable uvc function */
    ret = uvc_streaming_config(&uvc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uvc streaming config failed");
    }
    /* register the state callback to get connect/disconnect event
    * in the callback, we can get the frame list of current device
    */
    ESP_ERROR_CHECK(usb_streaming_state_register(&stream_state_changed_cb, NULL));
    /* start usb streaming, UVC and UAC MIC will start streaming because SUSPEND_AFTER_START flags not set */
    ESP_ERROR_CHECK(usb_streaming_start());
    ESP_ERROR_CHECK(usb_streaming_connect_wait(portMAX_DELAY));
    // wait for speaker device ready
    xEventGroupWaitBits(s_evt_handle, BIT3_SPK_START, false, false, portMAX_DELAY);

    while (1) {
        xEventGroupWaitBits(s_evt_handle, BIT3_SPK_START, true, false, portMAX_DELAY);
        /* Manually resume the speaker because SUSPEND_AFTER_START flags is set */
        ESP_ERROR_CHECK(usb_streaming_control(STREAM_UAC_SPK, CTRL_RESUME, NULL));
        usb_streaming_control(STREAM_UAC_SPK, CTRL_UAC_VOLUME, (void *)80);
        usb_streaming_control(STREAM_UAC_MIC, CTRL_UAC_VOLUME, (void *)80);
        ESP_LOGI(TAG, "speaker resume");
    }

    while (1) {
        vTaskDelay(100);
    }
}
