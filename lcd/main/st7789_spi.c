#include "st7789_spi.h"
#include "esp_heap_caps.h"
#include <string.h>

void spi_send_cmd_to_st7789(spi_device_handle_t spi, uint8_t cmd)
{
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd
    };
    gpio_set_level(SPI_DC_PIN, 0); // Command mode
    spi_device_polling_transmit(spi, &t);
}
void spi_send_data_to_st7789(spi_device_handle_t spi, const uint8_t* data, size_t len)
{
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data
    };
    gpio_set_level(SPI_DC_PIN, 1); // Data mode
    spi_device_polling_transmit(spi, &t);
}

void spi_lcd_set_window_canvas(spi_device_handle_t spi, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // Column Address Set (CASET)
    spi_send_cmd_to_st7789(spi, 0x2A);
    uint8_t caset[] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    spi_send_data_to_st7789(spi, caset, 4);

    // Page Address Set (PASET)
    spi_send_cmd_to_st7789(spi, 0x2B);
    uint8_t paset[] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    spi_send_data_to_st7789(spi, paset, 4);
}

void SPI_BUS_Init(spi_device_handle_t* spi_handle)
{
    // RST 控制
    gpio_set_direction(SPI_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SPI_RST_PIN, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level(SPI_RST_PIN, 1);
    vTaskDelay(120 / portTICK_PERIOD_MS);

    // SPI 配置
    spi_bus_config_t buscfg = {
        .mosi_io_num = SPI_MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = SPI_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * 2 + 8,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = SPI_CS_PIN,
        .queue_size = 1,
        .flags = SPI_DEVICE_NO_DUMMY,
    };

    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &devcfg, spi_handle);

    gpio_set_direction(SPI_DC_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SPI_CS_PIN, 1);
}

void st7789_hw_init(spi_device_handle_t spi_handle)
{
    spi_send_cmd_to_st7789(spi_handle, 0x11); // Sleep out
    vTaskDelay(120 / portTICK_PERIOD_MS);

    spi_send_cmd_to_st7789(spi_handle, 0x3A); // COLMOD
    uint8_t colmod = 0x55;
    spi_send_data_to_st7789(spi_handle, &colmod, 1);

    spi_send_cmd_to_st7789(spi_handle, 0x36); // MADCTL
    uint8_t madctl = 0x00; // RGB, normal scan
    spi_send_data_to_st7789(spi_handle, &madctl, 1);

    spi_send_cmd_to_st7789(spi_handle, 0x29); // Display on
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

void spi_image_to_lcd_st7789(spi_device_handle_t spi, const uint16_t* image)
{
    spi_lcd_set_window_canvas(spi, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    spi_send_cmd_to_st7789(spi, 0x2C); // Memory Write

    // 行缓冲区（自动 4 字节对齐）
    static uint8_t line_buf[LCD_WIDTH * 2];

    for (int y = 0; y < LCD_HEIGHT; y++) {
        // 转换一行：RGB565 小端 → 大端字节序
        for (int x = 0; x < LCD_WIDTH; x++) {
            uint16_t pixel = image[y * LCD_WIDTH + x];
            line_buf[x * 2 + 0] = (pixel >> 8) & 0xFF; // 高字节先发
            line_buf[x * 2 + 1] = pixel & 0xFF;        // 低字节后发
        }

        spi_send_data_to_st7789(spi, line_buf, LCD_WIDTH * 2);
    }
}
