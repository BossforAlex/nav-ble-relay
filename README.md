# 导航 BLE 转发 (NavBleRelay)

[![CI Build](https://github.com/BossforAlex/nav-ble-relay/actions/workflows/build.yml/badge.svg)](https://github.com/BossforAlex/nav-ble-relay/actions/workflows/build.yml)

一款 Android 应用（Flutter），用于读取**高德地图车机版（AmapAuto）**的公开导航广播，并通过**低功耗蓝牙（BLE）**实时转发给 **ESP32-S3 Super Mini + ST7789 TFT 屏幕**，实现 HUD 风格导航显示。

参考《[小屏手机手搓hud](https://www.bilibili.com/video/BV1yhcszwEUi)》视频设计：小屏 HUD 显示实时车速 + 导航引导，避免反复看中控。

---

## 功能特性

- **ESP32-S3 + TFT HUD 显示**：升级自 ESP32-C3，双核 240MHz + ST7789 SPI TFT 屏幕，HUD 风格 UI
- **屏幕自适应**：根据 TFT 分辨率自动缩放字体和布局（支持 240x240 / 170x320 等）
- **Flutter APP 重构**：Material 3 + 动态主题（Material You）+ google_fonts
- **实时导航数据转发**：引导信息、车道信息、路况光柱图、地图状态、定位信息
- **BLE GATT Server**：Android 设备作为 GATT Server，ESP32 作为 Central 连接并写入数据
- **HUD 配色**：高对比黑底配色，夜间行车友好
- **GitHub Actions 自动构建**：每次推送到 `main` 自动编译固件 + Flutter APK 并发布到 Release

---

## 架构

```
高德地图车机版 ──广播──> Flutter APP ──BLE──> ESP32-S3 + TFT 屏幕
                     (GATT Server)         (GATT Client + HUD)
```

### 项目结构

```
├── Auto/                    ESP32 固件（PlatformIO）
│   ├── src/
│   │   ├── ble/             BLE GATT Client
│   │   ├── nav/             导航数据解析
│   │   ├── screen/
│   │   │   ├── ScreenTFT.*  ST7789 TFT HUD 实现（S3）
│   │   │   └── ScreenConsole.* 串口虚拟屏幕（C3）
│   │   └── config/          配置
│   └── platformio.ini
├── app_flutter/             Flutter Android APP
│   ├── lib/
│   │   ├── ble/             BLE GATT Server
│   │   ├── protocol/        高德广播协议
│   │   ├── models/          数据模型
│   │   └── ui/              Material 3 界面
│   └── android/             原生层（广播接收 + BLE Server）
└── .github/workflows/       CI 构建
```

---

## 快速开始

### 前置条件

1. ESP32-S3 Super Mini（推荐）或 ESP32-C3 Super Mini
2. ST7789 SPI TFT 屏幕（240x240，S3 环境）
3. Android 5.0+ 设备，支持 BLE 外设模式
4. 安装高德地图车机版（AmapAuto）

### 刷写 ESP32-S3 固件

Release 中的 `firmware-esp32-s3-supermini.bin` 是**合并后的工厂镜像**：

```bash
# 擦除
esptool.py --chip esp32s3 --port COMx erase_flash

# 烧录（DIO + 40MHz + 8MB）
esptool.py --chip esp32s3 --port COMx --baud 460800 write_flash -z \
  --flash-mode dio --flash-freq 40m --flash-size 8MB \
  0x0 firmware-esp32-s3-supermini.bin
```

### 刷写 ESP32-C3 固件（向后兼容）

```bash
esptool.py --chip esp32c3 --port COMx erase_flash
esptool.py --chip esp32c3 --port COMx --baud 460800 write_flash -z \
  --flash-mode dio --flash-freq 40m --flash-size 4MB \
  0x0 firmware-esp32-c3-supermini.bin
```

> **重要**：ESP32-C3/S3 的 bootloader 位于 `0x0000`，必须使用 `0x0000` 地址烧录。

### 安装 Flutter APP

1. 打开仓库 [Releases](https://github.com/BossforAlex/nav-ble-relay/releases) 页面
2. 下载最新的 `app-flutter-release.apk`
3. 在 Android 设备上安装并运行

### 接线（ESP32-S3 + ST7789）

| ST7789 引脚 | ESP32-S3 引脚 | 说明 |
|------------|--------------|------|
| GND | GND | 地 |
| VCC | 3V3 | 电源 |
| SCL | GPIO12 | SPI 时钟 |
| SDA | GPIO11 | SPI 数据 |
| RES | GPIO14 | 复位 |
| DC | GPIO9 | 数据/命令 |
| CS | GPIO10 | 片选 |
| BLK | GPIO2 | 背光 |

### 使用步骤

1. 打开 Flutter APP，授予蓝牙和通知权限
2. 点击**启动服务**，APP 开始广播 BLE 并监听高德广播
3. 打开高德地图车机版并开始导航
4. ESP32-S3 自动连接名为 `ICA` 的 BLE 设备，接收 JSON 导航数据
5. TFT 屏幕显示 HUD 风格导航信息

---

## HUD 界面说明

ESP32-S3 TFT 屏幕显示内容（参考视频 HUD 设计）：

```
┌─────────────────────┐
│ ● ICA        帧计数  │  顶部状态栏（BLE 状态点 + 设备名）
│                     │
│        ▲            │  大号转向箭头（脉冲动画）
│                     │
│      350 m          │  路口距离（接近时变橙/红色）
│   当前路 → 下一路    │  路口信息
│                     │
│ ▓▓▓░░▓▓▓▓░░░▓       │  路况光柱（绿/黄/橙/红）
│                     │
│ 45   ┌─────┐        │  车速 + 限速标志
│ km/h │ 60  │        │  （超速时限速标志变红）
│      └─────┘        │
│ 12km / 25分  CAM 200m│  全程剩余 + 电子眼
└─────────────────────┘
```

---

## BLE 协议

Android 端作为 GATT Server，ESP32 作为 Client 连接并写入：

| 特征值 UUID | 数据类型 | 说明 |
|-------------|----------|------|
| `0000FFE1-...` | 引导信息 | 转向、距离、车速、限速等 |
| `0000FFE2-...` | 车道信息 | 接近路口时触发 |
| `0000FFE3-...` | 路况光柱 | 路线变化时触发 |
| `0000FFE4-...` | 导航状态 | 导航开始/结束/到达 |
| `0000FFE5-...` | 定位信息 | 方向、精度、速度 |

---

## 本地构建

### 固件

```bash
cd Auto
pio run -e esp32-s3-supermini    # ESP32-S3 + TFT
pio run -e esp32-c3-supermini    # ESP32-C3 串口版
```

### Flutter APP

```bash
cd app_flutter
flutter pub get
flutter build apk --release
# 输出: build/app/outputs/flutter-apk/app-release.apk
```

---

## 技术栈

- **固件**: ESP32-S3 + Arduino + TFT_eSPI + ArduinoJson
- **APP**: Flutter + Material 3 + Provider + flutter_blue_plus
- **CI**: GitHub Actions + PlatformIO + Flutter

## 许可证

[MIT License](LICENSE)
