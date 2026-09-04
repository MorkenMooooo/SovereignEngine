#include "lcd_driver.h"


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