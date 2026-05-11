# 🚀 SovereignEngine: 嵌入式AI智能硬件项目开发权威教程与知识库

> **作者**: Mooooo
> **中文名**: 门主
> **平台**: Bilibili & Douyin & 微信公众号(Wechat Official Account) @ 门主引擎 | Mooooo
> **官网**: https://www.SovereignEngine.cn
> **联系**: itsntcool@qq.com
> **许可证**: Apache License 2.0

![ESP32](https://img.shields.io/badge/Platform-ESP32-S3-blue?logo=espressif)
![License](https://img.shields.io/badge/License-Apache%202.0-green.svg)
![Status](https://img.shields.io/badge/Status-Active-brightgreen)

## 📖 项目简介

**SovereignEngine** 是一套基于 **ESP32-S3** 的嵌入式系统从零到一的实战项目集合。本项目从最基础的 GPIO 控制出发，逐步深入到 **FreeRTOS、UART、中断、I2C/SPI 通信、传感器集成、WiFi 连接、以及外设驱动 (LCD、ADC、定时器)** 等核心及进阶技术。

所有代码均遵循 **Apache 2.0** 开源协议，欢迎学习、引用和二次开发。

---

## 📂 项目目录与学习路径

本项目（截止于 2026 年 5 月 11 日）包含 **15 个循序渐进的实战示例**，并持续更新中。建议按编号顺序学习：

### 🔰 基础篇：GPIO 与 外设控制
| 编号 | 项目目录 | 核心技术点 |
| :--- | :--- | :--- |
| 01 | `01_LED` | GPIO 输出模式，点亮第一个 LED |
| 02 | `02_button_pullup` | GPIO 输入模式，内部上拉电阻配置 |
| 03 | `03_gpio_if` | 封装 GPIO 接口，提高代码复用性 |
| 04 | `04_button_and_LED` | 综合应用：输入检测 + 输出控制 |
| 05 | `05_gpio_config` | 驱动能力、上下拉、休眠唤醒配置 |
| 06 | `06_uart` | 串口打印与数据收发基础 |
| 10 | `10_pwm_01` / `10_pwm_02` / `10_pwm_03` / `10_rgb_PWM_02` | PWM 波形输出与 RGB 灯控制 |
| 10 | `10_gpio_rgb_ws2812` | LEDC 模块 PWM 输出，实现 WS2812 呼吸灯效果 |

### ⚙️ FreeRTOS基础篇：中断、定时器 与 实时系统
| 编号 | 项目目录 | 核心技术点 |
| :--- | :--- | :--- |
| 07 | `07_freeRTOS` | 任务创建、调度器启动、多任务并发 |
| 08 | `08_uart_intr` | 基于中断的高效串口数据接收 |
| 09 | `09_gpio_intr` | 按键中断触发，实时响应外部事件 |
| 12 | `12_esp_timer_interrupt_esp_idf` / `12_software_esp_timer_esp_idf` / `12_gptimer_esp_idf_hardware_timer` | 软件定时器与硬件定时器的使用 |

### 📡 通信篇：I2C / SPI / WiFi
| 编号 | 项目目录 | 核心技术点 |
| :--- | :--- | :--- |
| 11 | `11_i2c_AHT30` / `11_i2c_aht30_02` | AHT30 温湿度传感器 I2C 数据采集 |
| 11 | `11_BMP2803v3_01` / `11_i2c_bmp280_02` / `11_I2C_BMP280_tpa` / `11_bmp280_03_d` | BMP280 大气压强传感器 I2C 采集 |
| 11 | `11_BH1750_01` / `11_bh1750_esp_idf_driver` | BH1750 光照传感器 I2C 采集 |
| 11 | **`11_QMI8658a_01`** | **QMI8658A 六轴 IMU (加速度计+陀螺仪) 驱动与数据读取** |
| 13 | `13_st7789_lcd_01` | SPI 接口 LCD 屏幕驱动与显示 |
| 14 | `14_esp32s3_wifi_esp_idf` | ESP32-S3 WiFi 基础连接与扫描 |
| 14 | `14_esp32s3_wifi_http_led_esp_idf` / `14_esp32s3_wifi_http_button_led_esp_idf` | WiFi + HTTP 请求 + LED/按键远程控制 |
| 15 | `15_adc_01` | ADC 模数转换，读取模拟信号（如电位器） |

---

## 🛠️ 硬件与环境要求

- **开发板**：ESP32-S3 DevKit V1 或兼容型号
- **外设**：LED 灯、按键、各种传感器（温湿度、气压、光照、IMU）
- **电脑环境**：
  - Windows 11 / macOS / Linux
  - ESP-IDF (v5.5 或更高版本)
  - VS Code + Espressif IDF 插件

---

## 🚀 快速开始

### 1. 克隆项目

```bash
git clone https://github.com/MorkenMooooo/SovereignEngine.git
cd SovereignEngine
```

### 2. 配置 ESP-IDF
确保已安装 ESP-IDF 并正确配置环境变量。
```bash
# Linux/Mac
. $IDF_PATH/export.sh

# Windows
%IDF_PATH%\export.bat
```

### 3. 编译项目
```bash
cd 11_QMI8658a_01   // 请选择项目目录
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```
确保已将开发板串口设置为 COM3 或其他实际端口。