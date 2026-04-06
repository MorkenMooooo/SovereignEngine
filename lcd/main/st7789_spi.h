#ifndef __ST7789_SPI_H__
#define __ST7789_SPI_H__

#include "driver/spi_master.h"
#include "driver/gpio.h"

// 引脚定义（根据你的接线修改）
#define SPI_MOSI_PIN    11
#define SPI_SCLK_PIN    12
#define SPI_CS_PIN      10
#define SPI_DC_PIN      9
#define SPI_RST_PIN     8

// 屏幕尺寸
#define LCD_WIDTH   240
#define LCD_HEIGHT  320

// 函数声明（供 main.c 调用）
void SPI_BUS_Init(spi_device_handle_t* spi_handle);
void st7789_hw_init(spi_device_handle_t spi_handle);
void spi_send_cmd_to_st7789(spi_device_handle_t spi, uint8_t cmd);
void spi_send_data_to_st7789(spi_device_handle_t spi, const uint8_t* data, size_t len);
void spi_lcd_set_window_canvas(spi_device_handle_t spi, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

#endif
