# 导航 BLE 转发 (NavBleRelay)

[![Android CI Build](https://github.com/BossforAlex/nav-ble-relay/actions/workflows/build.yml/badge.svg)](https://github.com/BossforAlex/nav-ble-relay/actions/workflows/build.yml)

一款 Android 应用，用于读取**高德地图车机版（AmapAuto）**的公开导航广播，并通过**低功耗蓝牙（BLE）**实时转发给 ESP32 等外部设备。无需集成高德 SDK，纯本地读取广播数据。

---

## 功能特性

- **实时导航数据转发**：引导信息、车道信息、路况光柱图、地图状态、定位信息
- **BLE 外设模式**：Android 设备作为 GATT Server，ESP32 作为 Central 连接并订阅通知
- **Beline Moto 风格导航引导**：主界面提供大箭头转向预览 + 剩余距离 + 下条道路，适配后续 ESP32 小屏显示
- **Material 3 Expressive 主题**：支持浅色 / 深色模式，界面色彩遵循 Material 3 规范
- **前台服务保活**：服务在通知栏运行，支持启动 / 停止 / 自启动
- **GitHub Actions 自动构建**：每次推送到 `main` 自动编译 Debug APK 并发布到 Release

---

## 界面预览

主界面包含：

- 服务控制卡片（启动 / 停止 / 测试广播）
- 状态概览（BLE 连接、广播接收状态）
- **导航引导卡片**：大箭头根据下一转向自动旋转，显示剩余距离与下条道路名
- 导航状态、引导信息、车道指引、路况光柱、定位信息等详细数据面板

> 该布局同时作为 ESP32 屏幕显示的参考样式：深色背景、高对比度箭头、大号距离文字。

---

## 快速开始

### 前置条件

1. Android 5.0+ 设备，支持 BLE 外设模式
2. 安装高德地图车机版（AmapAuto）
3. 一台 ESP32 设备（作为 BLE Central）

### 刷写 ESP32 固件

Release 中的 `firmware-esp32-c3-supermini.bin` 是合并后的工厂镜像，可直接从 `0x0000` 地址烧录：

1. 下载 Release 中的 `firmware-esp32-c3-supermini.bin`。
2. 使用 **flash_download_tool_3.9.11**：
   - ChipType: `ESP32-C3`
   - WorkMode: `Develop`
   - SPI Speed: `40MHz`
   - SPI Mode: `DIO`
   - Flash Size: `32Mbit(4MB)`
   - 载入固件，地址填 `0x0000`
   - 选择正确串口，点击 **START** 烧录
3. 烧录完成后按一下板子 **RST** 重启。

> 若之前刷写过异常固件导致无法启动，建议先执行 `Erase` 或 `esptool.py erase_flash` 清空 Flash 后再烧录。

### 安装 APK

每次推送到 `main` 分支后，GitHub Actions 会自动构建 APK 并上传到 Release：

1. 打开仓库 [Releases](https://github.com/BossforAlex/nav-ble-relay/releases) 页面
2. 下载最新的 `app-debug.apk`
3. 在 Android 设备上安装并运行

### 使用步骤

1. 打开应用，授予蓝牙和通知权限。
2. （可选）进入 **设置**，填写 ESP32 的 MAC 地址开启白名单，防止附近其他 BLE 设备误连。
3. （可选）如果你的 ESP32 是 **ESP32-C3 Super Mini** 等小内存板，建议开启 **ESP32 简化模式**，Android 端会只发送转向、路口、距离等必要字段，降低解析负担。
4. 点击 **启动服务**，应用会在通知栏以前台服务运行。
5. 打开高德地图车机版并开始导航。
6. ESP32 扫描并连接名为 `ICA` 的 BLE 设备，订阅对应特征值通知后即可接收 JSON 导航数据。
7. 如蓝牙连接异常，可点击主界面右上角 **蓝牙日志** 图标查看实时 BLE 日志，方便排查问题。

---

## 本地构建

```bash
# 克隆仓库
git clone https://github.com/BossforAlex/nav-ble-relay.git
cd nav-ble-relay

# 构建 Debug APK
./gradlew assembleDebug

# 输出路径
app/build/outputs/apk/debug/app-debug.apk
```

### 环境要求

- JDK 17
- Android SDK API 35
- Gradle 8.7（Wrapper 自动下载）

---

## BLE 协议

Android 端作为 GATT Server，提供以下服务与特征值：

| 特征值 UUID | 数据类型 | 说明 |
|-------------|----------|------|
| `0000FFE1-...` | 引导信息 `GuideInfo` | 每秒更新，含转向、距离、车速、限速等 |
| `0000FFE2-...` | 车道信息 `DriveWayInfo` | 接近路口时触发 |
| `0000FFE3-...` | 路况光柱图 `TmcSegmentInfo` | 路线变化时触发 |
| `0000FFE4-...` | 地图状态 `MapState` | 导航开始 / 结束 / 到达 |
| `0000FFE5-...` | 定位信息 `LocationInfo` | 每秒更新 |

所有数据均以 JSON 格式通过 `notify` 发送：

```json
{
  "type": 10001,
  "ts": 1700000000000,
  "data": { ... }
}
```

---

## 主题与图标

- 界面采用 **Material 3 Expressive** 配色风格，完整适配浅色与深色模式。
- 导航转向图标、工具栏图标等来自 **[iconfont.cn](https://www.iconfont.cn/) 开源图标库**，遵循其开源使用协议。

---

## 技术栈

- Kotlin 1.9.24
- Android SDK 35
- Material Design 3
- BLE GATT Server
- AmapAuto 标准广播协议 20180813

---

## 贡献

欢迎提交 Issue 和 PR。

## 许可证

[MIT License](LICENSE)
