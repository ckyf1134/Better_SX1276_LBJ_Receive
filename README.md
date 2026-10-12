# SX1276_Receive_LBJ（通用版）

基于 **TTGO LoRa 32 v1.6.1（ESP32 + SX1276）** 的 LBJ 列车接近预警消息接收器。

本项目在 [RadioLib](https://github.com/jgromes/RadioLib) 的 `Pager_Receive.ino` 与
[LilyGo-LoRa-Series](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series) 模板基础上修改而来，
利用 SX1276 的 FSK 调制解调器接收中国铁路列车接近预警（LBJ，821.2375MHz，POCSAG 协议）消息。

> **⚠️ 免责声明（务必阅读）**
>
> 1. 本项目**仅供嵌入式编程与 sub-GHz 信号接收的学习交流**使用。
> 2. **严禁**在国家规定的铁路无线电频率上进行未经许可的接收与发射。这可能违反相关法律法规，
>    并可能面临严重处罚。使用者需自行承担全部后果。
> 3. 本项目为实验性项目，代码质量有限，可能存在未知问题，请自行承担使用风险。
> 4. 机车号等数据来源为公开出版物，本项目不包含任何涉密文件。
> 5. 如相关单位认为本项目存在不当之处，请通过仓库 Issue 联系，我们会及时处理。

---

## 功能特性

- 在 OLED 上显示格式化后的 LBJ 消息
- 将接收到的消息以**纯文本**和 **CSV（HTML 表格）** 两种格式保存到 TF 卡
- 内置 Telnet 服务器，可远程查看日志与状态
- BCH(31,21) 纠错，移植自 MMDVM_HS_Hat 的 `BCH3121.cpp`
- 无 TCXO 情况下的自动频率校正（AFC），基于 SX1276 的 FEI 频偏指示器
- 主用/备用双 WiFi 自动切换，后台非阻塞连接
- 设备无关：**不含任何 MAC 绑定、设备识别或授权校验逻辑**，任何 ESP32 板子都能编译运行

---

## 硬件

- 开发板：[TTGO LoRa 32 v1.6.1](https://www.lilygo.cc/products/lora3)
- 原理图：[T3_V1.6.1.pdf](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series/blob/master/schematic/T3_V1.6.1.pdf)
- TF 卡（可选，用于日志记录）

### 可选外设

> 以下均为可选功能，需要额外接线。

#### 1. DS3231 RTC（断电保持时间）

与板载 SSD1306 OLED 共用 IIC 总线：

```c
#define I2C_SDA   21
#define I2C_SCL   22
```

若已焊接 DS3231，请在 `src/utilities.h` 中取消注释：

```c
#define HAS_RTC // 已焊接外部 DS3231 模块
```

不使用 RTC 时，每次重启后从 NTP 服务器获取时间。

#### 2. 模拟按键（开发中）

由于 GPIO 数量不足，使用四键模拟按键：

```c
#define BUTTON_PIN   34
```

用于日志查看与设备设置等交互功能（开发中）。更多引脚定义见 `src/utilities.h`。

---

## 编译与烧录

### 1. 安装 PlatformIO

推荐使用 VS Code + [PlatformIO IDE 扩展](https://platformio.org/install/ide?install=vscode)，
或安装 [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)。

### 2. 配置 WiFi（**必做**）

所有用户配置集中在 **`src/common.h`**，编译前请修改为你自己的网络：

```c
#define WIFI_SSID            "YOUR_WIFI_SSID"        // 主用 WLAN 名称
#define WIFI_PASSWORD        "YOUR_WIFI_PASSWORD"    // 主用 WLAN 密码（8-16 位）
#define BACK_WIFI_SSID       "YOUR_BACKUP_SSID"      // 备用 WLAN 名称
#define BACK_WIFI_PASSWORD   "YOUR_BACKUP_PASSWORD"  // 备用 WLAN 密码（8-16 位）
```

同处还可修改首页显示内容：

```c
#define Logo1    " 中国铁路列车接近预警"   // 首页 LOGO 第一行
#define Logo2    "       通用版"          // 首页 LOGO 第二行
#define AdsStr   ""                       // 日志内自定义说明，留空则不输出
```

> 📌 **不要把你的真实 WiFi 密码提交到公开仓库。** 建议在本地保留自己的
> `common.h`，只向上游提交占位符版本。

### 3. 编译并烧录

```bash
pio run                 # 仅编译
pio run --target upload # 编译并烧录
pio device monitor      # 打开串口监视器（115200）
```

若依赖下载失败（`HTTPClientError`），见下方[常见问题](#依赖下载失败httpclienterror)。

---

## 配置说明

### SX1276 参数

| 项目 | 值 |
| --- | --- |
| 频率 | 821.2375MHz + 6 ppm（默认） |
| 模式 | FSK，RxDirect（DIO2） |
| 增益 | 001 + LnaBoostHf（AGC 关闭） |
| 接收带宽 | 12.5 kHz |

由于所用 SX1276 模块没有 TCXO，固件通过 SX1276 的频偏指示器（FEI）测量载波/前导码的
频率误差，收到载波或前导码后尝试锁定信号，以补偿晶振或发射机造成的频偏。
可通过串口命令 `afc off` 关闭该机制。

### 主要可调参数（`src/common.h`）

| 参数 | 说明 |
| --- | --- |
| `MaxCount` | 单次通电最大记录数，默认 65000 |
| `NearAlarm` | 主屏接近提醒开关 |
| `VerFlag` | `release` 正式版 / `test` 测试版 |
| `ValDate` | 测试版到期时间（仅 `VerFlag == "test"` 时生效） |
| `LogFlag` / `CSVFlag` | 文本日志 / CSV 日志开关 |

---

## 串口与 Telnet 命令

### 串口

| 命令 | 说明 |
| --- | --- |
| `ping` | 状态测试，返回 `pong` |
| `time` | 返回系统时间 |
| `sd end` / `sd begin` | 卸载 / 挂载 TF 卡 |
| `mem` | 返回剩余内存（`esp_get_free_heap_size()`） |
| `rst` | 返回复位原因 |
| `ppm read` | 返回当前 ppm |
| `afc on` / `afc off` | 启用 / 禁用自动频率校正 |

### Telnet

| 命令 | 说明 |
| --- | --- |
| `ping` | 状态测试，返回 `pong` |
| `read` | 从 TF 卡日志读取 1000 字节 |
| `log read [int]` | 从 TF 卡日志读取 `[int]` 字节 |
| `log status` | 返回 TF 日志是否启用 |
| `afc on` / `afc off` | 启用 / 禁用自动频率校正 |
| `ppm [float]` | 设置 ppm 为 `[float]` |
| `bat` | 返回电池电压 |
| `rssi` | 返回 SX1276 当前 RSSI |
| `gain` | 返回 SX1276 当前增益 |
| `time` | 返回系统时间 |
| `bye` | 断开 Telnet |

---

## 日志格式

TF 卡中同时生成纯文本日志与 CSV（HTML 表格）日志。
`test/sample_log.txt` 是一份**脱敏后的格式示例**（数值均为占位符）：

```
－－－－－－－－－－－－－－－－－－－－－－－
　　　　　　第     1 条记录
接收时间：20XX年XX月XX日XX时XX分XX秒
数据类型：LBJ第二页（扩展数据）
下行列车：0000次
运行速度： 52 KM/H
线路名称: XXX线
里程位置： XXX.X KM
机车注册号：00000000
列车类型：XX客车
信号强度：-101.0 dBm　　信号质量：一般
电池电压：  4.05伏特　　电量剩余：85％
－－－－－－－－－－－－－－－－－－－－－－－
```

---

## LBJ 扩展消息格式

该消息未出现在 `TB/T 3504-2018` 标准中。消息的正式名称与结构目前未知，
以下内容是通过实际接收比对推测得出。一条典型 LBJ 扩展消息包含：

| 半字节(4bit) | 编码 | 含义 |
| --- | --- | --- |
| 0-3 | ASCII | 类型 |
| 4-11 | 十进制 | 8 位机车登记号 |
| 12-13 | 未知 | 机车端：31 为 A 端，32 为 B 端，30 为无 A/B 端或未注册 |
| 14-29 | GB2312 | 路线 |
| 30-38 | 十进制 | 经度（XXX°XX.XXXX′ E） |
| 39-46 | 十进制 | 纬度（XX°XX.XXXX′ N） |
| 47-49 | 未知 | 通常为 000 |

共 50 个半字节 / 200 位，通过 10 个 POCSAG 帧传输。

---

## 常见问题

### 依赖下载失败（HTTPClientError）

首次编译需要从 GitHub 下载 **平台包**（`platform-espressif32`）与
**ESP32 Arduino 框架**（`framework-arduinoespressif32`，约 100+ MB）。
国内网络下这两步经常失败，典型报错：

```
Tool Manager: Installing platformio/framework-arduinoespressif32 @ ~3.20017.0
HTTPClientError:
```

> **重要**：PlatformIO 下载 GitHub 内容用的是内置的 Python HTTP 客户端，
> 它走的是系统代理/直连，**与 `git` 命令的通道不同**。因此"`git clone` 能通"
> 并不代表 PlatformIO 能通；只加速 git 的工具（如 Steam++ / Watt Toolkit）
> **无法**解决这个问题。

处理办法（按推荐顺序）：

**1. 给 PlatformIO 配置代理**（最有效）

```bash
# Windows PowerShell
$env:HTTPS_PROXY = "http://127.0.0.1:7890"   # 换成你自己代理的端口
$env:HTTP_PROXY  = "http://127.0.0.1:7890"
pio run
```

或在 `%USERPROFILE%\.platformio\platformio.ini` 中持久化配置。

**2. 使用国内镜像**

在 `%USERPROFILE%\.platformio\platformio.ini` 中加入：

```ini
[platformio]
; 走国内代理或镜像（地址请按你实际可用的镜像填写）
```

**3. 离线安装（无外网时）**

在能联网的机器上执行 `pio pkg install`，然后整个拷贝
`%USERPROFILE%\.platformio\packages` 与 `%USERPROFILE%\.platformio\platforms`
到目标机器。

**4. 确认平台版本**

本项目 `platformio.ini` 使用 `platform = espressif32`（不锁定版本），
会优先使用本机已缓存的平台。**若你把它改成 `espressif32@x.y.z`，
PlatformIO 会尝试从 GitHub 重新下载该版本 —— 在本机已缓存其他版本的情况下，
这反而会让编译失败。** 请在改动前确认目标版本可下载。

### 提示 `ModuleNotFoundError` / 找不到库

在 PlatformIO 中执行完整依赖安装：

```bash
pio pkg install
```

### 收到的消息是乱码或显示 `NA`、`********`

部分解码或损坏的消息会如实显示。出现 `<NUL>`、`NA`、`********` 或其中一部分，
说明该部分内容已损坏，属正常现象。

### 设备崩溃 / 重启

- **TF 卡热插拔不受支持**：通电状态下拔卡，会在下次收到消息时崩溃。请先断电再插拔。
- 供电不足也可能导致重启，建议使用稳定的 5V 电源。

---

## 已知问题

- TF 卡热插拔**不支持**。
- Telnet 服务**不支持多客户端**。
- 无蜂鸣器报警功能。
- 部分解码或损坏的消息仍会显示在屏幕上。
- WiFi 连接稳定性取决于路由器与信号环境。
- 模拟按键相关交互功能仍在开发中。

---

## 项目结构

```
src/
  main.cpp       主流程：初始化、接收循环、显示刷新
  networks.cpp   网络：WiFi 主备切换、NTP、Telnet、消息格式化
  sdlog.cpp      TF 卡日志（文本 + CSV/HTML）
  boards.cpp/.hpp  板级支持与引脚定义
  common.h       用户配置区（WiFi、显示、运行参数）
  utilities.h    工具函数与引脚定义
  BCH3121*.*     BCH(31,21) 纠错
  PagerMod.cpp   POCSAG 解码
  unicon.cpp     字库/图标
lib/RadioLib/    内嵌并裁剪的 RadioLib
partitions/      分区表（huge_app.csv）
test/            测试目录与脱敏日志示例
```

---

## 更新日志

见 [CHANGELOG.md](CHANGELOG.md)。

---

## 使用的开源库与参考

感谢所有作者与贡献者：

- [RadioLib](https://github.com/jgromes/RadioLib)（本项目内嵌并裁剪，MIT）
- [U8g2](https://github.com/olikraus/u8g2)
- [ESP32-Arduino](https://github.com/espressif/arduino-esp32)
- [PlatformIO](https://platformio.org/)
- [ESP32AnalogRead](https://github.com/madhephaestus/ESP32AnalogRead)
- BCH3121 来自 [POCSAG_HS](https://github.com/phl0/POCSAG_HS)
- [LilyGo-LoRa-Series](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series)
- [ESPTelnet](https://github.com/LennartHennigs/ESPTelnet)
- [RTClib](https://github.com/adafruit/RTClib)
- [Multimon-ng](https://github.com/EliasOenal/multimon-ng)
- 项目灵感来自 [GoRail_Pager](https://github.com/killeder/GoRail_Pager)

各第三方组件的许可与出处详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

---

## 许可证

本项目以 **MIT License** 发布，详见 [LICENSE](LICENSE)。
