# 第三方依赖与声明

Rongyi 原创代码及本次公开整理适用 [MIT License](LICENSE)。该许可证不替代 SDK、字体或其他上游组件的许可证、NOTICE、商标和服务条款。

| 组件 | 用途 | 许可证或边界 |
| :-- | :-- | :-- |
| ESP-IDF / FreeRTOS | ESP32-S3 驱动、网络、任务和构建框架 | ESP-IDF 由 PlatformIO `espressif32@6.13.0` 拉取；以 Espressif 当前发布的 Apache-2.0 与组件级声明为准：<https://github.com/espressif/esp-idf> |
| PlatformIO Core | 本地与 CI 构建编排 | 固定为 `6.1.19`；见 <https://platformio.org/> 与 <https://github.com/platformio/platformio-core> |
| Adafruit GFX 经典 5×7 字体 | `main/font_glcd.c` 中的 OLED ASCII 字模数据 | 文件头保留来源说明；请遵守 Adafruit GFX 的 BSD 许可与 NOTICE：<https://github.com/adafruit/Adafruit-GFX-Library> |

本仓不复制 ESP-IDF 或 PlatformIO 的完整上游源码。状态页仅使用浏览器/系统自带字体，不再向外部字体服务请求资源。硬件器件型号、模块商标和数据手册也分别属于其权利人。使用者应为自己选择的板卡、模块、驱动、电源和再分发内容单独核对许可与合规性。
