#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"


// 引脚定义
#define lcd_spi_cs     GPIO_NUM_10     // CS 片选引脚，当CS拉低时表示SPI从机设备被选中，可以进行数据传输
#define lcd_spi_dc     GPIO_NUM_14     // DC 数据/命令引脚，拉低表示数据，拉高表示命令
#define lcd_spi_rst    GPIO_NUM_13     // RST 复位引脚
#define lcd_spi_bl     GPIO_NUM_9      // BL LED 背光引脚
#define lcd_spi_mosi   GPIO_NUM_11     // MOSI （也是SDA）主输出引脚，串行数据线用于发送数据到屏幕
#define lcd_spi_clk    GPIO_NUM_12     // CLK 时钟引脚
#define spi_host       SPI2_HOST       // ESP32-S3 SPI 主机控制器选择


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


#endif