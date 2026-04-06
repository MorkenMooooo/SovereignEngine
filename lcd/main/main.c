#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "st7789_spi.h"


void app_main(void)
{
    spi_device_handle_t spi;
    SPI_BUS_Init(&spi);
    st7789_hw_init(spi);

    // 先清屏：全黑 (0x0000)
    spi_lcd_set_window_canvas(spi, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    spi_send_cmd_to_st7789(spi, 0x2C);
    gpio_set_level(SPI_DC_PIN, 1);

    static uint8_t black_pixel[2] = {0x00, 0x00};
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        spi_send_data_to_st7789(spi, black_pixel, 2);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS); // 等待清屏完成

    // 再填红色
    spi_lcd_set_window_canvas(spi, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    spi_send_cmd_to_st7789(spi, 0x2C);
    gpio_set_level(SPI_DC_PIN, 1);

    static uint8_t red_pixel[2] = {0xF8, 0x00};
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        spi_send_data_to_st7789(spi, red_pixel, 2);
    }
}