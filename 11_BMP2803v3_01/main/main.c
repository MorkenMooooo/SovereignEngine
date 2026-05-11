#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO    7
#define I2C_MASTER_SDA_IO    15
#define I2C_MASTER_FREQ_HZ   100000
#define BMP280_ADDR          0x76
#define I2C_TIMEOUT_MS       1000

#define BMP280_REG_CHIP_ID   0xD0
#define BMP280_REG_RESET     0xE0
#define BMP280_REG_CTRL_MEAS 0xF4
#define BMP280_REG_CONFIG    0xF5
#define BMP280_REG_DATA      0xF7
#define BMP280_REG_CALIB     0x88

typedef int32_t  BMP280_S32_t;
typedef uint32_t BMP280_U32_t;
typedef int64_t  BMP280_S64_t;

static const char *TAG = "BMP280";

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

static struct {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5;
    int16_t  dig_P6, dig_P7, dig_P8, dig_P9;
} calib;

static BMP280_S32_t t_fine;

static esp_err_t bmp280_write_reg(uint8_t reg_addr, uint8_t value)
{
    uint8_t buf[2] = {reg_addr, value};
    return i2c_master_transmit(dev_handle, buf, 2, I2C_TIMEOUT_MS);
}

static esp_err_t bmp280_read_reg(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, I2C_TIMEOUT_MS);
}

static esp_err_t bmp280_read_calib(void)
{
    uint8_t trim[24];
    esp_err_t ret = bmp280_read_reg(BMP280_REG_CALIB, trim, 24);
    if (ret != ESP_OK) return ret;

    calib.dig_T1 = (uint16_t)(trim[1] << 8 | trim[0]);
    calib.dig_T2 = (int16_t) (trim[3] << 8 | trim[2]);
    calib.dig_T3 = (int16_t) (trim[5] << 8 | trim[4]);
    calib.dig_P1 = (uint16_t)(trim[7] << 8 | trim[6]);
    calib.dig_P2 = (int16_t) (trim[9] << 8 | trim[8]);
    calib.dig_P3 = (int16_t) (trim[11] << 8 | trim[10]);
    calib.dig_P4 = (int16_t) (trim[13] << 8 | trim[12]);
    calib.dig_P5 = (int16_t) (trim[15] << 8 | trim[14]);
    calib.dig_P6 = (int16_t) (trim[17] << 8 | trim[16]);
    calib.dig_P7 = (int16_t) (trim[19] << 8 | trim[18]);
    calib.dig_P8 = (int16_t) (trim[21] << 8 | trim[20]);
    calib.dig_P9 = (int16_t) (trim[23] << 8 | trim[22]);

    ESP_LOGI(TAG, "校准参数读取成功");
    return ESP_OK;
}

static BMP280_S32_t bmp280_compensate_T(BMP280_S32_t adc_T)
{
    BMP280_S32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((BMP280_S32_t)calib.dig_T1 << 1))) *
            ((BMP280_S32_t)calib.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((BMP280_S32_t)calib.dig_T1)) *
              ((adc_T >> 4) - ((BMP280_S32_t)calib.dig_T1))) >> 12) *
            ((BMP280_S32_t)calib.dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T;
}

static BMP280_U32_t bmp280_compensate_P(BMP280_S32_t adc_P)
{
    BMP280_S64_t var1, var2, p;
    var1 = ((BMP280_S64_t)t_fine) - 128000;
    var2 = var1 * var1 * (BMP280_S64_t)calib.dig_P6;
    var2 = var2 + ((var1 * (BMP280_S64_t)calib.dig_P5) << 17);
    var2 = var2 + (((BMP280_S64_t)calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (BMP280_S64_t)calib.dig_P3) >> 8) +
           ((var1 * (BMP280_S64_t)calib.dig_P2) << 12);
    var1 = (((((BMP280_S64_t)1) << 47) + var1)) *
           ((BMP280_S64_t)calib.dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((BMP280_S64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((BMP280_S64_t)calib.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((BMP280_S64_t)calib.dig_P7) << 4);
    return (BMP280_U32_t)p;
}

static esp_err_t bmp280_init(void)
{
    esp_err_t ret;

    // 1. 验证芯片 ID
    uint8_t chip_id;
    ret = bmp280_read_reg(BMP280_REG_CHIP_ID, &chip_id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取芯片 ID 失败");
        return ret;
    }
    if (chip_id != 0x58) {
        ESP_LOGE(TAG, "芯片 ID 错误：期望 0x58，实际 0x%02X", chip_id);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "芯片 ID 验证成功：0x%02X", chip_id);

    // 2. 软复位
    ret = bmp280_write_reg(BMP280_REG_RESET, 0xB6);
    if (ret != ESP_OK) return ret;

    // 3. 等待 BMP280 完成 power-on-reset（NVM 加载需要时间）
    vTaskDelay(pdMS_TO_TICKS(100));

    // 4. 关键：重置 ESP32-S3 的 I2C 总线控制器
    // 软复位期间 BMP280 可能导致总线出现异常，需要恢复控制器状态
    i2c_master_bus_reset(bus_handle);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "软复位完成，I2C 总线已恢复");

    // 5. 读取校准参数（在 Sleep 模式下读取，安全）
    ret = bmp280_read_calib();
    if (ret != ESP_OK) return ret;

    // 不写 CONFIG，保持默认值
    // 不启动测量，保持 Sleep 模式，每次读取时触发 Forced 模式
    ESP_LOGI(TAG, "BMP280 初始化完成（Sleep 模式待命）");
    return ESP_OK;
}

static esp_err_t bmp280_read_data(float *temperature_c, float *pressure_pa)
{
    esp_err_t ret;

    // 1. 触发一次 Forced 模式测量
    //    osrs_t=010(x2), osrs_p=101(x16), mode=01(Forced)
    ret = bmp280_write_reg(BMP280_REG_CTRL_MEAS, 0x55);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "触发 Forced 模式失败，等待 100ms 后重试...");
        vTaskDelay(pdMS_TO_TICKS(100));
        ret = bmp280_write_reg(BMP280_REG_CTRL_MEAS, 0x55);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "重试仍然失败: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "重试成功");
    }

    // 2. 固定等待 80ms，确保测量完成（最大 43.2ms）
    vTaskDelay(pdMS_TO_TICKS(80));

    // 3. 一次性连续读取 6 字节（burst read，保证数据一致性）
    uint8_t raw[6];
    ret = bmp280_read_reg(BMP280_REG_DATA, raw, 6);
    if (ret != ESP_OK) return ret;

    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);
    
    // 4. 必须先计算温度（t_fine 供气压补偿使用）
    BMP280_S32_t temp_raw = bmp280_compensate_T(adc_T);
    BMP280_U32_t press_raw = bmp280_compensate_P(adc_P);

    *temperature_c = (float)temp_raw / 100.0f;
    *pressure_pa   = (float)press_raw / 256.0f;

    return ESP_OK;
}

static esp_err_t bmp280_verify_registers(void)
{
    uint8_t ctrl_meas, config_reg, status_reg;
    esp_err_t ret;

    ret = bmp280_read_reg(0xF3, &status_reg, 1);  // STATUS
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取 STATUS 寄存器失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "STATUS (0xF3) = 0x%02X (measuring=%d, im_update=%d)",
             status_reg, (status_reg >> 3) & 1, status_reg & 1);

    ret = bmp280_read_reg(0xF4, &ctrl_meas, 1);  // CTRL_MEAS
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取 CTRL_MEAS 寄存器失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "CTRL_MEAS (0xF4) = 0x%02X (expected 0x57)", ctrl_meas);

    ret = bmp280_read_reg(0xF5, &config_reg, 1);  // CONFIG
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取 CONFIG 寄存器失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "CONFIG (0xF5) = 0x%02X (expected 0x00)", config_reg);

    return ESP_OK;
}



void app_main(void)
{
    ESP_LOGI(TAG, "程序启动");

    // ---- 初始化 I2C 主机总线 ----
    i2c_master_bus_config_t bus_config = {
        .i2c_port             = I2C_NUM_0,
        .sda_io_num           = I2C_MASTER_SDA_IO,
        .scl_io_num           = I2C_MASTER_SCL_IO,
        .clk_source           = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt    = 7,
        .flags.enable_internal_pullup = false,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));
    ESP_LOGI(TAG, "I2C 总线初始化完成");

    // ---- 添加 BMP280 设备到总线 ----
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BMP280_ADDR,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));
    ESP_LOGI(TAG, "BMP280 设备添加完成");

    // ---- 扫描 I2C 总线（必须保留，确保总线正常工作）----
    ESP_LOGI(TAG, "扫描 I2C 总线...");
    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        if (i2c_master_probe(bus_handle, addr, 200) == ESP_OK) {
            ESP_LOGI(TAG, "✅发现 I2C 设备: 0x%02X", addr);
        }
    }
    ESP_LOGI(TAG, "I2C 扫描完成!");

    // ---- 初始化 BMP280 ----
    ESP_ERROR_CHECK(bmp280_init());

    // 不要用 ESP_ERROR_CHECK，避免 abort
    // esp_err_t verify_ret = bmp280_verify_registers();
    // if (verify_ret != ESP_OK) {
    //     ESP_LOGE(TAG, "寄存器验证失败: %s", esp_err_to_name(verify_ret));
    //     // 不 abort，继续观察
    // }
    

    // ---- 循环读取数据 ----
    while (1) {
        float temperature, pressure;
        esp_err_t ret = bmp280_read_data(&temperature, &pressure);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "温度: %.2f °C | 气压: %.2f Pa (%.2f hPa)",
                     temperature, pressure, pressure / 100.0f);
        } else {
            ESP_LOGE(TAG, "读取数据失败: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}