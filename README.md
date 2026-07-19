# 基于ESP32-S3的多传感器智能安全监测系统

> 面向嵌入式学习的 ESP32-S3 多传感器监测与低压执行器联动原型：MQ-2/MQ-5 原始 ADC 采样、火焰模块输入、超声波雷达、SSD1306 OLED、本地状态页与分时执行器逻辑。

[![Validate](https://github.com/rongyishuaige7/esp32-s3-smart-safety-monitor/actions/workflows/validate.yml/badge.svg)](https://github.com/rongyishuaige7/esp32-s3-smart-safety-monitor/actions/workflows/validate.yml)
[![License: MIT](https://img.shields.io/badge/Code-MIT-f97316.svg)](LICENSE)

> [!CAUTION]
> 这是本科软硬件学习原型，**不是火灾报警器、可燃气体报警器、自动灭火系统、通风安全系统、生命安全设备或生产控制设备**。MQ-2/MQ-5 原始 ADC 值、火焰模块输入和代码阈值不能替代人工核实、合规检测、报警送达或紧急处置。遇到真实火情、疑似燃气泄漏或其他危险，请立刻离开风险区域并联系当地专业应急服务。

## 项目资料

相关 EDA 文件已整理，文件处理说明见 [MEDIA_EVIDENCE](docs/MEDIA_EVIDENCE.md)。

## 原型学习范围

- ESP32-S3、ESP-IDF、FreeRTOS 任务与互斥共享状态；
- MQ-2 / MQ-5 的 ADC 原始数据采样与代码阈值/锁存逻辑；
- 数字火焰模块、HC-SR04 超声波、SSD1306 I²C OLED；
- 蜂鸣器、LED、低压水泵、L9110 风扇与两路 SG90 PWM 的分时驱动；
- 构建时内嵌的本地 HTML 状态页和 SSE；

## 任务因果链

```text
MQ-2 / MQ-5 原始 ADC、火焰模块、HC-SR04
  → FreeRTOS 传感器任务与共享状态
  → 代码阈值、锁存和固定时长的教学逻辑
  → OLED / 本地 SSE 状态页
  → LED、蜂鸣器、低压水泵、风扇、舵机（仅受控演示）
```

代码中的“烟雾”“气体”“火焰”“开窗”“水泵”等标签只描述输入和低压原型逻辑；没有任何一个环节被当作真实安全判断、告警送达或自动处置承诺。

## 硬件与 GPIO

| 模块/信号 | GPIO | 接线说明 |
| :-- | --: | :-- |
| MQ-2 AO | 1 | ADC1_CH0；未校准原始值 |
| MQ-5 AO | 2 | ADC1_CH1；未校准原始值 |
| 火焰模块 DO | 4 | 低电平触发 |
| HC-SR04 TRIG / ECHO | 5 / 6 | 5 V ECHO 必须分压/电平转换 |
| 蜂鸣器 / LED | 7 / 15 | 蜂鸣器为低电平有效 |
| 水泵 H 桥 INA / INB | 16 / 17 | 仅低压驱动意图；供电和管路待确认 |
| L9110 风扇 IA / IB | 18 / 8 | 仅低压风扇驱动意图 |
| 窗户 / 雷达舵机 | 9 / 10 | 50 Hz PWM；须独立供电并共地 |
| SSD1306 OLED SDA / SCL | 11 / 12 | I²C `0x3C`、100 kHz 假定 |

查看完整元件、接线和上电风险：[BOM](hardware/BOM.csv)、[接线图](hardware/wiring-diagram.svg)、[硬件与电气说明](HARDWARE.md)。所有实际电压、电流、电机驱动、上拉、电平转换和共地均须按模块资料核对。

## 构建

已验证工具链：**PlatformIO Core 6.1.19**、`espressif32@6.13.0`、ESP-IDF 框架，板型 `esp32-s3-devkitc-1`，Flash 配置 8 MB。

```bash
git clone https://github.com/rongyishuaige7/esp32-s3-smart-safety-monitor.git
cd esp32-s3-smart-safety-monitor
pio run
```

首次构建会下载 PlatformIO 平台、ESP-IDF 框架和交叉工具链。构建时，`tools/embed_web.py` 会从 `main/web/index.html` 在 `.pio/generated/` 构建目录生成 C 数组；生成文件不进入 Git。

一键在隔离副本中运行公开门禁和构建：

```bash
bash scripts/verify.sh
```

若要烧录，先阅读 [HARDWARE.md](HARDWARE.md) 和 [构建说明](docs/VERIFICATION.md)。**首次通电时不要连接水泵、风扇、舵机或任何高风险负载；原型启动自检会依次驱动这些接口。**

## 本地网络状态页

公开源码刻意不携带固定 SSID 或密码，默认也不会启动 Wi-Fi/HTTP。

只有在隔离可信测试网络中，才可执行：

```bash
cp main/local_config.example.h main/local_config.h
# 编辑 main/local_config.h，填写仅用于本地测试的热点名称和至少 8 位密码
pio run
```

`main/local_config.h` 已被 Git 忽略，不得提交、截图或写入日志。启用后固件提供本地 WPA2 SoftAP、`/` 页面和 `/api/status` SSE；它**无认证、没有 TLS、没有设备身份、没有访问控制**，不得暴露公网。状态页展示设备当前传送的原始样本和软件状态。

## 阈值与执行器逻辑

代码使用以下原始 ADC 阈值：

| 逻辑项 | 源码常量 | 默认值 | 说明 |
| :-- | :-- | --: | :-- |
| MQ-2 一级 / 二级 / 三级 | `THRESHOLD_SMOKE_LEVEL1/2/3` | 800 / 1500 / 2200 | 原始 ADC 教学阈值。 |
| MQ-5 原始值 | `THRESHOLD_GAS_PPM_RAW` | 2000 | 原始 ADC 值，不是 ppm。 |

阈值和定时只应用于教学逻辑。改动它们后需要重新检查电气接线、受控低压演示与所有文档，不得将“水泵/风扇启动”描述为灭火或安全处置成功。

## 目录

```text
main/                  ESP-IDF 应用、传感器、执行器、OLED 与本地状态页
main/local_config.example.h  本地热点配置模板；真实 local_config.h 不入库
hardware/              BOM 与源码推导的接线边界图
docs/                  来源、状态、验证、协议和 GitHub 元数据
scripts/               Secret scan、发布结构检查与一键验证
tests/                 无硬件源码契约测试
.github/workflows/     固定依赖的 CI 构建与 Artifact 证据
```

## 注意事项

- `font_glcd.c` 包含 Adafruit GFX 的经典 5×7 字体数据，相关声明见 [第三方说明](THIRD_PARTY_NOTICES.md)；
- CI Artifact 用于保留构建记录，保留期为 14 天。

## 许可证与学术诚信

Rongyi 原创整理和源码使用 [MIT License](LICENSE)。ESP-IDF、FreeRTOS、Adafruit 字体和其他上游组件仍遵循各自许可证与声明，见 [第三方说明](THIRD_PARTY_NOTICES.md)。

欢迎学习、复现和二次开发；请注明来源，不要将本仓直接冒充为自己的毕业设计成果。使用者须自行对接线、测试、合规和任何现实风险负责。
