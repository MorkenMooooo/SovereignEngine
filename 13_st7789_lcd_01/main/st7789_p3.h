#ifndef ST7789_P3_H
#define ST7789_P3_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

/**
 * @brief Simple example to demonstrate how to use the SPI master driver and GPIO mux.
 * This code is a more complex version of the hello_spi example.
 *
 * GPIO assignment:
 *
 * SPI CLK    GPIO 12
 * SPI MOSI   GPIO 11
 * SPI CS     GPIO 10
 * BL LED     GPIO 9
 * RST        GPIO 13
 * DC         GPIO 14
 */

 #define lcd_spi_cs     GPIO_NUM_10     // CS 片选引脚，当CS拉低时表示SPI从机设备被选中，可以进行数据传输
 #define lcd_spi_dc     GPIO_NUM_14     // DC 数据/命令引脚，拉低表示数据，拉高表示命令
 #define lcd_spi_rst    GPIO_NUM_13     // RST 复位引脚
 #define lcd_spi_bl     GPIO_NUM_9      // BL LED 背光引脚
 #define lcd_spi_mosi   GPIO_NUM_11     // MOSI （也是SDA）主输出引脚，串行数据线用于发送数据到屏幕
 #define lcd_spi_clk    GPIO_NUM_12     // CLK 时钟引脚
 #define spi_host       SPI2_HOST       // ESP32-S3 SPI 主机控制器选择



void lcd_gpio_init(void);                              // LCD GPIO初始化函数，配置GPIO引脚模式和功能
void st7789_init(spi_device_handle_t spi_handle);      // ST7789屏幕初始化函数，发送初始化命令和数据到屏幕
void st7789_write_cmd(uint8_t cmd, spi_device_handle_t spi_handle);    // 发送命令函数，向屏幕发送命令数据
void st7789_write_data(uint8_t data, spi_device_handle_t spi_handle);  // 发送数据函数，向屏幕发送数据

#endif // ST7789_P3_H
