# 第三方组件与许可声明

本项目包含或引用了以下第三方代码。各组件版权归其原作者所有，按其各自许可发布。

## 内嵌代码

| 组件 | 位置 | 许可 | 出处 |
| --- | --- | --- | --- |
| RadioLib | `lib/RadioLib/` | MIT | https://github.com/jgromes/RadioLib |

> `lib/RadioLib/` 为内嵌副本，并已按本项目需要裁剪（见 `platformio.ini` 中的
> `RADIOLIB_EXCLUDE_*` 编译选项）。原库的许可文件保留在该目录内的 `license.txt`（MIT）。

## 移植的代码片段

| 组件 | 位置 | 来源 | 许可说明 |
| --- | --- | --- | --- |
| BCH3121 纠错 (`BCH3121.cpp/.hpp/.c/.h`) | `src/` | MMDVM_HS_Hat / [POCSAG_HS](https://github.com/phl0/POCSAG_HS) | ⚠️ 上游为 GPL-3.0 |

> **注意**：`src/BCH3121*` 系从 GPL-3.0 项目移植而来。若你打算以更宽松的许可
> 分发本项目，请先确认该部分代码的许可兼容性，或将其替换为独立实现。
> 详见 `src/BCH3121.md`。

## 通过 PlatformIO 引用的依赖

以下依赖在构建时下载，不随本仓库分发，各自遵循其原始许可：

| 依赖 | 许可 | 出处 |
| --- | --- | --- |
| ESP Telnet | MIT | https://github.com/LennartHennigs/ESPTelnet |
| U8g2 | BSD-2-Clause | https://github.com/olikraus/u8g2 |
| ESP32AnalogRead | MIT | https://github.com/madhephaestus/ESP32AnalogRead |
| RTClib | MIT | https://github.com/adafruit/RTClib |
| Arduino core for ESP32 | LGPL-2.1 | https://github.com/espressif/arduino-esp32 |

## 参考项目与灵感来源

- [LilyGo-LoRa-Series](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series) — 板级模板
- [Multimon-ng](https://github.com/EliasOenal/multimon-ng) — POCSAG 协议参考
- [GoRail_Pager](https://github.com/killeder/GoRail_Pager) — 项目灵感

感谢以上所有作者与贡献者。
