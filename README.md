\# 🚀 SovereignEngine: 嵌入式AI智能硬件项目开发权威教程与嵌入式开发者的知识库



> \*\*作者\*\*: Mooooo  

> \*\*ChineseName\*\*: 门主  

> \*\*平台\*\*: Bilibili \& Douyin \& 微信公众号(Wechat Official Account) @ 门主引擎 | Mooooo  

> \*\*联系\*\*: itsntcool@qq.com  

> \*\*许可证\*\*: Apache License 2.0



!\[ESP32](https://img.shields.io/badge/Platform-ESP32-blue?logo=espressif)

!\[License](https://img.shields.io/badge/License-Apache%202.0-green.svg)

!\[Status](https://img.shields.io/badge/Status-Active-brightgreen)



\## 📖 项目简介



\*\*SovereignEngine\*\* 是一套基于 \*\*ESP32-S3\*\* 的嵌入式系统学习与实战项目集合。本项目从最基础的 GPIO 控制出发，逐步深入到自由实时操作系统 (FreeRTOS)、UART 通信、中断处理等核心嵌入式技术。



无论你是嵌入式初学者，还是希望复习 ESP32-S3 底层驱动的开发者，这里都有你需要的代码示例和最佳实践。所有代码均遵循 \*\*Apache 2.0\*\* 开源协议，欢迎学习、引用和二次开发。



\## ✨ 核心功能与示例



本项目包含 9 个循序渐进的实战示例：



| 编号 | 示例名称 | 核心技术点 |

| :--- | :--- | :--- |

| \*\*01\*\* | 🟢 LED 控制 | GPIO 输出模式，点亮你的第一个灯 |

| \*\*02\*\* | 🔘 按键上拉输入 | GPIO 输入模式，内部上拉电阻配置 |

| \*\*03\*\* | ⚙️ GPIO 抽象层 | 封装 GPIO 接口，提高代码复用性 |

| \*\*04\*\* | 💡 按键控灯 | 综合应用：输入检测 + 输出控制 |

| \*\*05\*\* | 🛠️ GPIO 高级配置 | 驱动能力、上下拉、休眠唤醒配置 |

| \*\*06\*\* | 📡 UART 通信 | 串口打印与数据收发基础 |

| \*\*07\*\* | 🔄 FreeRTOS 入门 | 任务创建、调度器启动、多任务并发 |

| \*\*08\*\* | ⚡ UART 中断驱动 | 基于中断的高效串口数据接收 |

| \*\*09\*\* | 🚨 GPIO 外部中断 | 按键中断触发，实时响应外部事件 |



\## 🛠️ 硬件要求



\- \*\*开发板\*\*: ESP32-S3 DevKit V1 或兼容型号

\- \*\*下载器\*\*: 内置或外接 USB-to-TTL 模块

\- \*\*外设\*\*: LED 灯、按键、杜邦线若干

\- \*\*开发环境\*\*: 

&#x20; - ESP-IDF (v5.5 或更高版本推荐)

&#x20; - VS Code + EIM + ESP-IDF 插件 + Windows11



\## 🚀 快速开始



\### 1. 克隆项目

```bash

git clone https://github.com/MorkenMooooo/SovereignEngine.git

cd SovereignEngine


\### 2. 配置环境
``` 确保已安装 ESP-IDF 并正确配置环境变量。

.  $ IDF\_PATH/export.sh  # Linux/Mac

%IDF\_PATH%\\export.bat  # Windows




