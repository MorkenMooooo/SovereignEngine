#include <stdio.h>
#include "st7789_p3.h"



spi_device_handle_t spi_handle;


void app_main(void)
{   

    lcd_gpio_init();  // 初始化LCD GPIO引脚
    
    // SPI bus initialization
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = lcd_spi_mosi,
        .sclk_io_num = lcd_spi_clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 320 * 2 + 8,
        
        
        
    };
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE("ST7789", "Failed to initialize SPI bus");
        return;
    };

    // 配置SPI设备参数结构体
    spi_device_interface_config_t spi_dev_cfg = {
        .clock_source = SPI_CLK_SRC_DEFAULT, // 使用默认时钟源
        .clock_speed_hz = 40 * 1000 * 1000,  // SPI时钟频率，根据屏幕规格书，最高可达62.5MHz，但实际使用中可能需要调整以确保稳定性
        .spics_io_num = lcd_spi_cs,          // SPI片选引脚
        .mode = 0,                           // SPI模式，CLK polarity: 0, CLK phase: 0，时钟极性说的是空闲状态时的CLK信号电平
        .queue_size = 7,
    };

    // 将SPI设备添加到SPI总线上
    ret = spi_bus_add_device(SPI2_HOST, &spi_dev_cfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("ST7789", "Failed to add SPI device");
        return;
    }



    st7789_init(spi_handle);    // 初始化ST7789屏幕
    
}
