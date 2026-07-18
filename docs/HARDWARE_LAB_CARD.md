# Hardware Lab 索引卡片草案

## 名称

```text
基于ESP32-S3的多传感器智能安全监测系统
```

## 摘要

```text
ESP32-S3 多传感器监测教学原型，包含 MQ-2/MQ-5 原始采样、火焰与超声波输入、OLED、本地状态页和低压执行器分时逻辑。
```

## 平台

```text
ESP32-S3 · ESP-IDF · PlatformIO · FreeRTOS · MQ-2 · MQ-5 · SSD1306 · HC-SR04
```

## 真实状态口径

```text
源码来源、公开净化、无硬件源码契约与 ESP32-S3 固件构建已验证；当前开发板、MQ-2、MQ-5、火焰、HC-SR04、OLED、蜂鸣器、泵、风扇、舵机与本地网络状态页尚未按当前公开 commit 重新真机复测。
```

## 公开范围与边界

```text
当前未公开实物照片、演示视频、EDA、PCB、Gerber 或制造文件；已公开 BOM、源码推导接线边界图、来源、协议、状态与验证说明。固定热点密码不公开；用户本地启用的 SoftAP/SSE 无认证、无 TLS。该原型不是火灾/燃气报警、自动灭火、通风安全或生命安全设备。
```

填入 Hardware Lab 前，必须将 `head_sha`、固定 Actions URL 与最终公开仓 default-branch exact HEAD 同步；线上 CI 完成前不得写成构建证据已通过。

- **历史媒体 / EDA：** 已加入经脱敏的历史衍生材料；范围和版本差异见 [`MEDIA_EVIDENCE.md`](MEDIA_EVIDENCE.md)。它们不证明当前公开提交已完成真机复测。
