#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "QMI8658A";

// 引脚定义 (请根据你的实际接线修改)
#define I2C_MASTER_SCL_IO          GPIO_NUM_42
#define I2C_MASTER_SDA_IO          GPIO_NUM_41
#define I2C_MASTER_FREQ_HZ         400000         // 400kHz
#define I2C_PORT                   I2C_NUM_0
#define QMI8658_ADDR               0x6B           // AD0 = connected GND, 7-bit地址
#define I2C_TIMEOUT_MS             1000

// 寄存器地址
#define REG_WHO_AM_I               0x00
#define REG_CTRL1                  0x02
#define REG_CTRL2                  0x03
#define REG_CTRL3                  0x04
#define REG_CTRL5                  0x06
#define REG_CTRL7                  0x08
#define REG_STATUS0                0x2E
#define REG_ACC_DATA               0x35
#define REG_GYRO_DATA              0x3B
#define REG_RESET                  0x60

// 全局I2C句柄
static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

// 写寄存器
static esp_err_t qmi8658_write_reg(uint8_t reg_addr, uint8_t data) {
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_TIMEOUT_MS);
}

// 读寄存器
static esp_err_t qmi8658_read_reg(uint8_t reg_addr, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, I2C_TIMEOUT_MS);
}

void app_main(void) {
    ESP_LOGI(TAG, "启动 QMI8658A 简化测试");

    // 1. 初始化 I2C 主机总线
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false, // 关闭内部上拉
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    // 2. 添加设备
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));

    // 3. 检查 WHO_AM_I
    uint8_t who_am_i = 0;
    ESP_ERROR_CHECK(qmi8658_read_reg(REG_WHO_AM_I, &who_am_i, 1));
    if (who_am_i != 0x05) {
        ESP_LOGE(TAG, "WHO_AM_I 错误: 0x%02X", who_am_i);
        vTaskDelete(NULL);
    } else {
        ESP_LOGI(TAG, "✅ QMI8658A 连接成功!");
    }

    // 4. 软件复位
    qmi8658_write_reg(REG_RESET, 0xB0);
    vTaskDelay(pdMS_TO_TICKS(10));

    // 5. 基础配置 (1024dps量程, 250Hz, 开启加速度计和陀螺仪)
    qmi8658_write_reg(REG_CTRL1, 0x40); // 地址自增, 小端字节序 (Little Endian) - 匹配 ESP32-S3
    qmi8658_write_reg(REG_CTRL2, 0x25); // 加速度计: 8g, 250Hz
    qmi8658_write_reg(REG_CTRL3, 0x65); // 陀螺仪: 1024dps, 224.2Hz
    qmi8658_write_reg(REG_CTRL5, 0x00); // 禁用低通滤波
    qmi8658_write_reg(REG_CTRL7, 0x03); // 使能传感器 (Bit0=ACC, Bit1=GYRO)
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "初始化完成，开始读取数据...");

    // 6. 循环读取数据
    while (1) {
        uint8_t status;
        qmi8658_read_reg(REG_STATUS0, &status, 1);
        
        // 检查 StatusS0: Bit0=加速度计数据就绪, Bit1=陀螺仪数据就绪
        // 0x03 = 0b00000011，表示两个传感器都有新数据
        if ((status & 0x03) == 0x03) {
            uint8_t raw_data[12];
            
            // 读取加速度计数据 (Register 0x35-0x3A): AX_L, AX_H, AY_L, AY_H, AZ_L, AZ_H
            qmi8658_read_reg(REG_ACC_DATA, raw_data, 12); // 连续读取加速度计和陀螺仪数据，利用地址自增功能
            
            // 读取陀螺仪数据 (Register 0x3B-0x40): GX_L, GX_H, GY_L, GY_H, GZ_L, GZ_H
            // qmi8658_read_reg(REG_GYRO_DATA, raw_data + 6, 6);


            // 解析原始数据 (小端字节序: 低字节_L在前，高字节_H在后)
            short acc[3], gyro[3];
            for (int i = 0; i < 3; i++) {
                // 加速度计: raw_data[0]=AX_L, raw_data[1]=AX_H, raw_data[2]=AY_L, ...
                acc[i] = (short)((raw_data[i*2+1] << 8) | raw_data[i*2]);
                
                // 陀螺仪: raw_data[6]=GX_L, raw_data[7]=GX_H, raw_data[8]=GY_L, ...
                gyro[i] = (short)((raw_data[6 + i*2+1] << 8) | raw_data[6 + i*2]);
            }

            // 转换为物理单位
            // 加速度计: 8g 量程, 16位有符号数范围 ±32768
            // 灵敏度 = 32768 / 8 = 4096 LSB/g
            float acc_x = (float)acc[0] / 4096.0f;
            float acc_y = (float)acc[1] / 4096.0f;
            float acc_z = (float)acc[2] / 4096.0f;
            
            // 陀螺仪: 1024dps 量程, 16位有符号数范围 ±32768
            // 灵敏度 = 32768 / 1024 = 32 LSB/dps
            float gyro_x = (float)gyro[0] / 32.0f;
            float gyro_y = (float)gyro[1] / 32.0f;
            float gyro_z = (float)gyro[2] / 32.0f;

            ESP_LOGI(TAG, "ACC(g): X=%+.3f Y=%+.3f Z=%+.3f | GYRO(dps): X=%+.2f Y=%+.2f Z=%+.2f", 
                     acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // 10Hz 采样率 (100ms间隔)
    }
}