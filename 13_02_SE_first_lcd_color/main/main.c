#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "photo_SE01.h"

#include "font.h"


extern const uint8_t image_data[];  // 在 image.h 中定义

// 引脚定义
#define lcd_spi_cs     GPIO_NUM_10     // CS 片选引脚，当CS拉低时表示SPI从机设备被选中，可以进行数据传输
#define lcd_spi_dc     GPIO_NUM_14     // DC 数据/命令引脚，拉低表示数据，拉高表示命令
#define lcd_spi_rst    GPIO_NUM_13     // RST 复位引脚
#define lcd_spi_bl     GPIO_NUM_9      // BL LED 背光引脚
#define lcd_spi_mosi   GPIO_NUM_11     // MOSI （也是SDA）主输出引脚，串行数据线用于发送数据到屏幕
#define lcd_spi_clk    GPIO_NUM_12     // CLK 时钟引脚
#define spi_host       SPI2_HOST       // ESP32-S3 SPI 主机控制器选择


spi_device_handle_t spi_device_handle;


// 函数声明
void lcd_gpio_init(void);
void st7789_init(spi_device_handle_t spi_device_handle);
void st7789_write_cmd(uint8_t cmd, spi_device_handle_t spi_device_handle);
void st7789_write_data(uint8_t data, spi_device_handle_t spi_device_handle);
void st7789_set_window(uint16_t x0, uint16_t y0, 
                        uint16_t x1, uint16_t y1, 
                        spi_device_handle_t spi_device_handle);
void lcd_fill_color_cpu(uint16_t color, spi_device_handle_t spi_handle);
void lcd_fill_color_dma(uint16_t color, spi_device_handle_t spi_handle);
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color, spi_device_handle_t spi_handle);
void lcd_display_image_dma(spi_device_handle_t spi, const uint8_t *image_data);
void lcd_show_char(uint16_t x, uint16_t y, char ch, 
                   uint16_t fg, uint16_t bg, 
                   spi_device_handle_t spi);
void lcd_show_string(uint16_t x, uint16_t y, const char *str,
                     uint16_t fg, uint16_t bg,
                     spi_device_handle_t spi);


/**
 * @brief 主函数
 */
void app_main(void)
{   
    lcd_gpio_init();


    // SPI bus initialization
    spi_bus_config_t spi_buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = lcd_spi_mosi,
        .sclk_io_num = lcd_spi_clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 320 * 2 + 8,   
        
    };
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &spi_buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE("ST7789", "Failed to initialize SPI bus");
        return;
    };

    // 配置SPI设备参数结构体
    spi_device_interface_config_t spi_st7789_dev_cfg = {
        .clock_source = SPI_CLK_SRC_DEFAULT, // 使用默认时钟源
        .clock_speed_hz = 40 * 1000 * 1000,  // SPI时钟频率，根据屏幕规格书，最高可达62.5MHz，但实际使用中可能需要调整以确保稳定性
        .spics_io_num = lcd_spi_cs,          // SPI片选引脚
        .mode = 0,                           // SPI模式，CLK polarity: 0, CLK phase: 0，时钟极性说的是空闲状态时的CLK信号电平
        .queue_size = 7,
    };

    // 将SPI设备添加到SPI总线上
    ret = spi_bus_add_device(SPI2_HOST, &spi_st7789_dev_cfg, &spi_device_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("ST7789", "Failed to add SPI device");
        return;
    }


    // 初始化屏幕
    st7789_init(spi_device_handle);

    // 填充屏幕（0x801F 紫色， 0x07E0 绿色， 0xF800 红色， 0xFFFF 白色）
    lcd_fill_color_cpu(0x9C60, spi_device_handle);


    // 定义颜色数组（顺序：红黄蓝黑白绿橙粉棕紫）
    // uint16_t colors[] = {
    //     0xF800,  // 红
    //     0xFFE0,  // 黄
    //     0x001F,  // 蓝
    //     0x0000,  // 黑
    //     0xFFFF,  // 白
    //     0x07E0,  // 绿
    //     0xFC00,  // 橙
    //     0xF81F,  // 粉
    //     0x9C60,  // 棕
    //     0x801F   // 紫
    // };
    // int color_count = sizeof(colors) / sizeof(colors[0]);   // 获取颜色数组的长度
    // int index = 0;  // 颜色索引

    // // 循环填充颜色（10种颜色）
    // while (1) {
    //     lcd_fill_color_dma(colors[index], spi_device_handle);  // 调用填充颜色函数，发送颜色数据给屏幕）
    //     index = (index + 1) % color_count;  // 发送完一种颜色后，将索引加1，并取余数，确保索引在颜色数组范围内
    //     vTaskDelay(pdMS_TO_TICKS(500));  // 延时0.5秒
    // }


    // 绘制一个点
    lcd_draw_pixel(100, 100, 0xFFFF, spi_device_handle); // 在(100,100)画一个白点


    // 显示图片
    // lcd_display_image_dma(spi_device_handle, image_data);
    // // 保持显示
    // while (1) {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }

    // 清屏（可选），用黑色背景
    // lcd_fill_color_dma(0x0000, spi_device_handle);  // 黑色

    // // 显示一句英文
    // lcd_show_string(10, 20, "Hello, SovereignEngine!", 0xFFFF, 0x0000, spi_device_handle);
    // lcd_show_string(10, 50, "Menzhu Oh Yeah!", 0x07E0, 0x0000, spi_device_handle); // 绿色前景

    // // 保持显示
    // while (1) {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }

}










/**
 * @brief GPIO初始化
 */
void lcd_gpio_init(void){
    
    // GPIO配置结构体
    gpio_config_t lcd_gpio_config = {
        .pin_bit_mask = (1ULL << lcd_spi_dc) | (1ULL << lcd_spi_rst) | (1ULL << lcd_spi_bl),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
        
    };
    // GPIO初始化
    gpio_config(&lcd_gpio_config);

    // 初始化完成后，默认将DC、RST、BL引脚设置为高电平，确保屏幕处于非复位状态并且背光打开
    gpio_set_level(lcd_spi_dc, 1);     // 默认数据模式
    gpio_set_level(lcd_spi_rst, 1);    // 默认复位状态
    gpio_set_level(lcd_spi_bl, 1);     // 默认背光开启

}




/**
 * @brief 发送命令
 *
 * @param cmd 命令
 * @param spi_handle SPI句柄
 */
void st7789_write_cmd(uint8_t cmd, spi_device_handle_t spi_device_handle)
{
    // 引脚拉低，表示command，Page48 D/CX indicates whether the byte is command (D/CX=’0’) 
    // or parameter/RAM data (D/CX=’1’).
    gpio_set_level(lcd_spi_dc, 0);
    // 发送命令
    spi_transaction_t transmit_cmd_config = {
        .tx_buffer = &cmd,
        .length = 8,
    };
    spi_device_polling_transmit(spi_device_handle, &transmit_cmd_config);
}



/**
 * @brief 发送数据
 *
 * @param data 数据
 * @param spi_handle SPI句柄
 */
void st7789_write_data(uint8_t data, spi_device_handle_t spi_device_handle)
{
    // 引脚拉高，表示数据，Page48 D/CX indicates whether the byte is command (D/CX=’0’) 
    // or parameter/RAM data (D/CX=’1’).
    gpio_set_level(lcd_spi_dc, 1);
    // 发送数据
    spi_transaction_t transmit_data_config = {
        .tx_buffer = &data,
        .length = 8,
    };
    spi_device_polling_transmit(spi_device_handle, &transmit_data_config);
}




/**
 * @brief SPI初始化，配置SPI总线参数并添加SPI设备
 * 
 */
void st7789_init(spi_device_handle_t spi_device_handle){ 
    // 1， 硬件复位，拉低RST引脚
    // 屏幕初始化代码
    gpio_set_level(lcd_spi_rst, 0);     // 硬件复位，拉低RST引脚，默认高电平
    // 延时10ms，Page42 ↓
    // 原文 7. It is necessary to wait 5msec after releasing RESX before sending commands. Also Sleep Out command cannot be sent for 120msec.
    vTaskDelay(pdMS_TO_TICKS(10));

    gpio_set_level(lcd_spi_rst, 1);     // 硬件复位，拉高，恢复高电平
    vTaskDelay(pdMS_TO_TICKS(120));     
    // 延时120ms，Page42 ↓
    // 原文 3. During the Resetting period, the display will be blanked 
    // (The display is entering blanking sequence, which maximum time is 120 ms, when Reset Starts in Sleep Out –mode. 
    
    // 2， 软件复位，发送0x01命令
    // 发送软件复位命令0x01，Page124
    st7789_write_cmd(0x01, spi_device_handle);
    vTaskDelay(pdMS_TO_TICKS(120));

    // 3， 退出睡眠模式，发送0x11命令
    st7789_write_cmd(0x11, spi_device_handle);
    vTaskDelay(pdMS_TO_TICKS(10));     // 从睡眠模式退出后并立即再次进入睡眠模式要等120毫秒，而退出后发送其他新命令要等待至少5毫秒

    // 4， 设置像素格式，发送0x3A命令，参数0x55表示16位颜色以及262K，Page185，
    st7789_write_cmd(0x3A, spi_device_handle);
    st7789_write_data(0x55, spi_device_handle);

    // 5，设置显示方向，发送0x36命令，参数0x00表示竖屏、RGB模式，Page176
    st7789_write_cmd(0x36, spi_device_handle);
    st7789_write_data(0x00, spi_device_handle);

    // 6, 设置颜色反转开启 0x21
    st7789_write_cmd(0x21, spi_device_handle);

    // 7，设置display on，发送0x29命令，Page124
    st7789_write_cmd(0x29, spi_device_handle);

    vTaskDelay(pdMS_TO_TICKS(100));    // 等待100ms，确保屏幕稳定显示

    ESP_LOGI("ST7789", "ST7789 init success!");
}




/**
 * @brief 设置 ST7789 的显示窗口（列/行地址范围）
 * @param x0 起始列（0~239）
 * @param y0 起始行（0~319）
 * @param x1 结束列（0~239）
 * @param y1 结束行（0~319）
 * @param spi_handle SPI 设备句柄
 */
void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, spi_device_handle_t spi_handle)
{
    // ---- 设置列地址（X 方向） ----
    st7789_write_cmd(0x2A, spi_handle);          // 告诉屏幕：接下来我要发列地址参数

    // 发送起始列（高字节，低字节）
    st7789_write_data((x0 >> 8) & 0xFF, spi_handle); // 起始列的高 8 位
    st7789_write_data(x0 & 0xFF, spi_handle);        // 起始列的低 8 位

    // 发送结束列（高字节，低字节）
    st7789_write_data((x1 >> 8) & 0xFF, spi_handle); // 结束列的高 8 位
    st7789_write_data(x1 & 0xFF, spi_handle);        // 结束列的低 8 位

    // ---- 设置行地址（Y 方向） ----
    st7789_write_cmd(0x2B, spi_handle);          // 告诉屏幕：接下来我要发行地址参数

    // 发送起始行（高字节，低字节）
    st7789_write_data((y0 >> 8) & 0xFF, spi_handle);
    st7789_write_data(y0 & 0xFF, spi_handle);

    // 发送结束行（高字节，低字节）
    st7789_write_data((y1 >> 8) & 0xFF, spi_handle);
    st7789_write_data(y1 & 0xFF, spi_handle);
}





/**
 * @brief 填充整个屏幕为指定颜色（RGB565）
 * @param color 16位颜色值
 * @param spi_device_handle SPI设备句柄
 */
void lcd_fill_color_cpu(uint16_t color, spi_device_handle_t spi_device_handle)
{
    // 1. 设置全屏窗口 (0,0) ~ (239,319)
    st7789_set_window(0, 0, 239, 319, spi_device_handle);

    // 2. 发送写内存命令 (0x2C)
    st7789_write_cmd(0x2C, spi_device_handle);

    // 3. 计算像素总数
    uint32_t pixel_count = 240 * 320;  // 76800 个像素

    // 4. 逐个字节发送颜色数据（每个像素2字节）
    uint8_t color_high = (color >> 8) & 0xFF;
    uint8_t color_low  = color & 0xFF;

    for (uint32_t i = 0; i < pixel_count; i++) {
        st7789_write_data(color_high, spi_device_handle);
        st7789_write_data(color_low, spi_device_handle);
    }
}


/**
 * @brief 填充整个屏幕为指定颜色（DMA）
 * @param color 16位颜色值
 * @param spi_device_handle SPI设备句柄
 */

void lcd_fill_color_dma(uint16_t color, spi_device_handle_t spi_handle)
{
    // 第1步：设置全屏窗口
    st7789_set_window(0, 0, 239, 319, spi_handle);

    // 第2步：发送写内存命令
    st7789_write_cmd(0x2C, spi_handle);

    // 第3步：DC 引脚拉高（数据模式）
    gpio_set_level(lcd_spi_dc, 1);

    // 第4步：申请一块 DMA 内存作为"快递车"
    // 不能一次发整屏，硬件有单次传输上限，所以分块发送
    int chunk_size = 4096; // 每次发 4096 字节（约 2048 个像素）
    uint8_t *buf = heap_caps_malloc(chunk_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (buf == NULL) {
        ESP_LOGE("ST7789", "DMA内存申请失败！");
        return;
    }

    // 第5步：把颜色数据填满缓冲区
    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;
    for (int i = 0; i < chunk_size; i += 2) {
        buf[i]     = hi;
        buf[i + 1] = lo;
    }

    // 第6步：循环分块发送，直到整屏填满
    int total = 240 * 320 * 2; // 153600 字节
    int sent  = 0;

    while (sent < total) {
        // 计算本次发多少字节（最后一块可能不足 4096）
        int this_time = total - sent;
        if (this_time > chunk_size) this_time = chunk_size;

        spi_transaction_t t = {
            .tx_buffer = buf,
            .length    = this_time * 8, // 单位是 bit，所以 ×8
        };
        spi_device_transmit(spi_handle, &t);

        sent += this_time;
    }

    // 第7步：释放内存
    heap_caps_free(buf);
}




// void lcd_fill_color_dma(uint16_t color, spi_device_handle_t spi_handle)
// {
//     st7789_set_window(0, 0, 239, 319, spi_handle);
//     st7789_write_cmd(0x2C, spi_handle);
//     gpio_set_level(lcd_spi_dc, 1);

//     // chunk_size 最大设为 32768（ESP32-S3 硬件单次 DMA 上限）
//     int chunk_size = 32768;
//     uint8_t *buf = heap_caps_malloc(chunk_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
//     if (buf == NULL) {
//         ESP_LOGE("ST7789", "DMA内存申请失败！");
//         return;
//     }

//     uint8_t hi = (color >> 8) & 0xFF;
//     uint8_t lo = color & 0xFF;
//     for (int i = 0; i < chunk_size; i += 2) {
//         buf[i]     = hi;
//         buf[i + 1] = lo;
//     }

//     int total = 240 * 320 * 2; // 153600 字节
//     int sent  = 0;
//     while (sent < total) {
//         int this_time = total - sent;
//         if (this_time > chunk_size) this_time = chunk_size;

//         spi_transaction_t t = {
//             .tx_buffer = buf,
//             .length    = this_time * 8,
//         };
//         spi_device_transmit(spi_handle, &t);
//         sent += this_time;
//     }

//     heap_caps_free(buf);
// }



/**
 * @brief 绘制一个像素
 * @param x 横坐标
 * @param y 纵坐标
 * @param color 16位颜色值
 * @param spi_device_handle SPI设备句柄
 */
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color, spi_device_handle_t spi_handle)
{
    // 设置窗口为 (x,y) 到 (x,y)，即一个像素
    st7789_set_window(x, y, x, y, spi_handle);
    // 发送写内存命令
    st7789_write_cmd(0x2C, spi_handle);
    // 发送颜色数据（高字节、低字节）
    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;
    st7789_write_data(hi, spi_handle);
    st7789_write_data(lo, spi_handle);
}






/**
 * @brief 显示一张 240x320 的图片（RGB565）
 * @param spi SPI设备句柄
 * @param image_data 图片数据指针（必须按行顺序排列，每个像素高字节在前）
 */
void lcd_display_image_dma(spi_device_handle_t spi, const uint8_t *image_data)
{
    // 1. 设置全屏窗口
    st7789_set_window(0, 0, 239, 319, spi);

    // 2. 发送写内存命令
    st7789_write_cmd(0x2C, spi);

    // 3.DC 引脚拉高（数据模式）
    gpio_set_level(lcd_spi_dc, 1);

    // 4. 计算总数据量
    const uint32_t total_bytes = 240 * 320 * 2;  // 153600

    // 5. 分块发送，每次最多 4096 字节（可根据硬件调整）
    const uint32_t chunk_size = 4096;
    uint32_t sent = 0;

    while (sent < total_bytes) {
        uint32_t remaining = total_bytes - sent;
        uint32_t this_time = (remaining > chunk_size) ? chunk_size : remaining;

        spi_transaction_t trans = {
            .tx_buffer = image_data + sent,   // 从当前位置取数据
            .length = this_time * 8,          // 单位：位
        };
        spi_device_transmit(spi, &trans);     // 使用 DMA

        sent += this_time;
    }
}



/**
 * @brief 在指定位置显示一个 8x16 的字符
 * @param x     左上角横坐标 (0~239)
 * @param y     左上角纵坐标 (0~319)
 * @param ch    要显示的字符（例如 'A'）
 * @param fg    前景色（16位RGB565）
 * @param bg    背景色（16位RGB565）
 * @param spi   SPI设备句柄
 */
void lcd_show_char(uint16_t x, uint16_t y, char ch, 
                   uint16_t fg, uint16_t bg, 
                   spi_device_handle_t spi)
{
    // 1. 在 idxArray 中查找字符的位置（下标）
    int index = 0;
    for (int i = 0; idxArray[i] != '\0'; i++) {
        if (idxArray[i] == ch) {
            index = i;
            break;
        }
    }
    // 如果找不到，就显示空格（idxArray[0] 是空格）
    if (idxArray[index] != ch) index = 0;

    // 2. 设置窗口：宽度是 8（x+7），高度是 16（y+15）
    st7789_set_window(x, y, x + 7, y + 15, spi);

    // 3. 发送“写内存”命令
    st7789_write_cmd(0x2C, spi);

    // 4. 拉高 DC 引脚，表示接下来发的是像素数据
    gpio_set_level(lcd_spi_dc, 1);

    // 5. 关键改动：每个字符只占 16 个字节（因为 16行 × 1字节/行）
    uint8_t *font_ptr = fontArray + index * 16; 

    // 6. 逐行扫描（一共16行）
    for (int row = 0; row < 16; row++) {
        // 每行只取 1 个字节（8个 bit）
        uint8_t byte = font_ptr[row];

        // 7. 逐列扫描（一共8列，因为宽度是8）
        for (int col = 0; col < 8; col++) {
            // 判断当前列对应的 bit 是 1 还是 0
            // (7 - col) 表示高位在前（最左边是 bit7）
            uint8_t bit = (byte >> (7 - col)) & 0x01;

            // 选择颜色：bit=1 用前景色，bit=0 用背景色
            uint16_t color = bit ? fg : bg;

            // 发送颜色（高字节在前）
            uint8_t hi = (color >> 8) & 0xFF;
            uint8_t lo = color & 0xFF;
            st7789_write_data(hi, spi);
            st7789_write_data(lo, spi);
        }
    }
}


void lcd_show_string(uint16_t x, uint16_t y, const char *str,
                     uint16_t fg, uint16_t bg,
                     spi_device_handle_t spi)
{
    while (*str) {
        lcd_show_char(x, y, *str, fg, bg, spi);
        x += 8;  // ⬅️ 这里改成 8
        str++;
    }
}