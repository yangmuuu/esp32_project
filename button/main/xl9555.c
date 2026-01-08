#include "xl9555.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h" // 事件组

#define XL9555_ADDR 0x20

static i2c_master_bus_handle_t xl9555_bus_handle = NULL;
static i2c_master_dev_handle_t xl9555_dev_handle = NULL;

static xl9555_input_cb_t    xl9555_io_callback = NULL;
 
static EventGroupHandle_t   xl9555_io_event = NULL; // 全局事件组

static gpio_num_t xl9555_int_io;

#define XL9555_ISR_BIT  BIT0
// 中断服务函数
// 让代码放在 RAM，防止 Flash 被缓存锁死
static void IRAM_ATTR xl9555_int_handle(void * param)
{
    // 发出通知，在别的任务中读取值
    BaseType_t taskWake; // 通知中断退出后是否进行任务切换
    xEventGroupSetBitsFromISR(xl9555_io_event, XL9555_ISR_BIT, &taskWake);
    portYIELD_FROM_ISR(taskWake); // 上下文切换请求
}

static void xl9555_task(void* param)
{
    uint16_t last_input;
    EventBits_t ev;
    xl9555_read_word(0x00, &last_input); // 先读一次
    while (1)
    {
        ev = xEventGroupWaitBits(xl9555_io_event, XL9555_ISR_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if(ev&XL9555_ISR_BIT)
        {
            // 有电平变化了
            uint16_t input = 0;
            esp_rom_delay_us(1000); // 减少毛刺干扰
            if(gpio_get_level(xl9555_int_io) != 0)
            {
                continue;
            }
            esp_err_t ret = xl9555_read_word(0x00, &input);
            if(ret == ESP_OK)
            {
                // 遍历，看看那些位发生了变化
                for (int i = 0; i < 16; i++)
                {
                    uint8_t last_value = last_input&(1<<i)?1:0;
                    uint8_t value = input&(1<<i)?1:0;
                    if(value != last_value && xl9555_io_callback)
                    {
                        xl9555_io_callback((1<<i), value);
                    }
                }
            }

            // 更新最新值
            last_input = input;
        }
    }
    
}


// 初始化 I2C 和 XL9555 器件
esp_err_t xl9555_init(gpio_num_t sda, gpio_num_t scl, gpio_num_t _int, xl9555_input_cb_t f)
{
    // 1 使能I2C总线
    i2c_master_bus_config_t bus_config =
        {
            .clk_source = I2C_CLK_SRC_DEFAULT, // 默认时钟源
            .sda_io_num = sda,
            .scl_io_num = scl,
            .glitch_ignore_cnt = 7, // 如果线路上的故障周期小于此值，则可以过滤掉，通常值为7
            .i2c_port = 1, // s3有两个i2c,0和1,-1表示系统自行分配
            // .flags =
            // {
            //     .enable_internal_pullup = 1, // 使能上拉 （硬件已经上拉，可以不写）
            // },
        };

    // 初始化并申请一条i2c总线，并返回句柄
    i2c_new_master_bus(&bus_config, &xl9555_bus_handle);

    // 2 添加设备

    i2c_device_config_t dev_config =
        {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = XL9555_ADDR,
            .scl_speed_hz = 100000, // 时钟频率

        };

    // 挂载子设备，返回子设备句柄
    i2c_master_bus_add_device(xl9555_bus_handle, &dev_config, &xl9555_dev_handle);
    xl9555_io_callback = f;
    if(_int != GPIO_NUM_NC)
    {
        // 创建事件组
        xl9555_io_event = xEventGroupCreate();
        xl9555_int_io = _int;
        // 初始化

        gpio_config_t _int_config =
        {
            .intr_type = GPIO_INTR_NEGEDGE,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pin_bit_mask = (1<<_int)
        };
        gpio_config(&_int_config);

        // 使能全局中断初始化
        gpio_install_isr_service(0);
        // 注册回调
        gpio_isr_handler_add(_int, xl9555_int_handle, (void*)_int);

        // 新建任务函数
        xTaskCreatePinnedToCore(xl9555_task, "xl9555", 4096, NULL, 3, NULL, 1);
    }

    return ESP_OK;
}

// 写入两个字节数据
esp_err_t xl9555_write_word(uint8_t reg, uint16_t data)
{
    // 写数据
    uint8_t write_buf[3];
    write_buf[0] = reg;
    write_buf[1] = data & 0xff;
    write_buf[2] = (data >> 8) & 0xff;
    return i2c_master_transmit(xl9555_dev_handle, write_buf, 3, 500);
}

// 读取两个字节
esp_err_t xl9555_read_word(uint8_t reg, uint16_t *data)
{
    uint8_t addr[1];
    addr[0] = reg;
    return i2c_master_transmit_receive(xl9555_dev_handle, addr, 1, (uint8_t *)data, 2, 500);
}

esp_err_t xl9555_ioconfig(uint16_t config)
{
    // I²C 是“开漏 + 上拉”的总线，外面只要有个尖峰、毛刺、上拉不够强，或者 XL9555 正忙，就可能 NACK/超时，返回 ESP_ERR_TIMEOUT 之类的错误。
    // 硬件层次上这种概率很低，但不为 0；而方向寄存器一旦写错，整颗芯片的 IO 口就全成输入或全成输出，后果比丢一次数据严重得多。
    // 0x06 和 0x07挨着，所以直接写16位，会自动滑过去
    esp_err_t ret;
    do
    {
        // 没写成功就一直执行
        ret = xl9555_write_word(0x06, config);
        vTaskDelay(pdMS_TO_TICKS(150));
    } while (ret != ESP_OK);
    return ret;
}

int xl9555_pin_read(uint16_t pin)
{
    uint16_t data = 0;
    xl9555_read_word(0x00, &data);
    return (data&pin)?1:0;
}

// 写引脚
esp_err_t xl9555_pin_write(uint16_t pin, int level)
{
    // 先读取原来的值
    uint16_t data = 0;
    xl9555_read_word(0x02, &data);
    if(level) // 高电平
    {
        data |= pin;
    }
    else
    {
        data &= ~pin; // 清零
    }
    return xl9555_write_word(0x02, data);
}