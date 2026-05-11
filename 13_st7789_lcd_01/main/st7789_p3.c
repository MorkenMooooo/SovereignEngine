#include "st7789_p3.h"


/**
 * @brief 发送命令
 *
 * @param cmd 命令
 * @param spi_handle SPI句柄
 */
void st7789_write_cmd(uint8_t cmd, spi_device_handle_t spi_handle)
{
    // 引脚拉低，表示数据，Page48 D/CX indicates whether the byte is command (D/CX=’0’) 
    // or parameter/RAM data (D/CX=’1’).
    gpio_set_level(lcd_spi_dc, 0);
    // 发送命令
    spi_transaction_t transmit_config = {
        .tx_buffer = &cmd,
        .length = 8,
    };
    spi_device_polling_transmit(spi_handle, &transmit_config);
}


/**
 * @brief 发送数据
 *
 * @param data 数据
 * @param spi_handle SPI句柄
 */
void st7789_write_data(uint8_t data, spi_device_handle_t spi_handle)
{
    // 引脚拉高，表示数据，Page48 D/CX indicates whether the byte is command (D/CX=’0’) 
    // or parameter/RAM data (D/CX=’1’).
    gpio_set_level(lcd_spi_dc, 1);
    // 发送数据
    spi_transaction_t transmit_config = {
        .tx_buffer = &data,
        .length = 8,
    };
    spi_device_polling_transmit(spi_handle, &transmit_config);
}



/**
 * @brief GPIO初始化，配置GPIO引脚模式和功能
 * 
 */
void lcd_gpio_init(void){
    
    // GPIO配置结构体
    gpio_config_t lcd_gpio_config = {
        .pin_bit_mask = (1ULL << lcd_spi_dc) | (1ULL << lcd_spi_rst) | (1ULL << lcd_spi_bl),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
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
 * @brief SPI初始化，配置SPI总线参数并添加SPI设备
 * 
 */
void st7789_init(spi_device_handle_t spi_handle){ 
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
    st7789_write_cmd(0x01, spi_handle);
    vTaskDelay(pdMS_TO_TICKS(120));

    // 3， 退出睡眠模式，发送0x11命令
    st7789_write_cmd(0x11, spi_handle);
    vTaskDelay(pdMS_TO_TICKS(10));     // 从睡眠模式退出后并立即再次进入睡眠模式要等120毫秒，而退出后发送其他新命令要等待至少5毫秒

    // 4， 设置像素格式，发送0x3A命令，参数0x55表示16位颜色以及262K，Page185，
    st7789_write_cmd(0x3A, spi_handle);
    st7789_write_data(0x55, spi_handle);

    // 5，设置显示方向，发送0x36命令，参数0x00表示竖屏、RGB模式，Page176
    st7789_write_cmd(0x36, spi_handle);
    st7789_write_data(0x00, spi_handle);

    // 6，设置display on，发送0x29命令，Page124
    st7789_write_cmd(0x29, spi_handle);

    vTaskDelay(pdMS_TO_TICKS(100));    // 等待100ms，确保屏幕稳定显示
}


void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, spi_device_handle_t spi_handle){
    
}