#include "lcd_driver.h"
#include "photo_SE01.h"



extern const uint8_t image_data[];  // 在 image.h 中定义


// SPI句柄
spi_device_handle_t spi_device_handle;


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


    // 显示图片
    lcd_display_image_dma(spi_device_handle, image_data);
    // 保持显示
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }


}