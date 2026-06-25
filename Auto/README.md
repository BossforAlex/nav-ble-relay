# AutoNavDisplay

[![Build ESP32 Firmware](https://github.com/BossforAlex/nav-ble-relay/actions/workflows/build.yml/badge.svg)](https://github.com/BossforAlex/nav-ble-relay/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/BossforAlex/nav-ble-relay?label=release)](https://github.com/BossforAlex/nav-ble-relay/releases)

ESP32-C3 Super Mini 导航信息接收与显示项目（PlatformIO）。

> **快速下载固件**：访问 [Releases](https://github.com/BossforAlex/nav-ble-relay/releases) 页面，下载对应板型的 `firmware-*.bin`，使用 esptool 或 PlatformIO 直接烧录。

## 项目特点

- **可移植**：通过 `Screen` 抽象层解耦显示驱动，当前用串口虚拟屏幕调试，后续可无缝替换为 OLED/LCD/TFT。
- **模块化**：BLE、协议解析、渲染逻辑分层清晰，便于单独测试和修改。
- **调试友好**：所有关键状态通过串口输出，无需屏幕即可验证数据链路；支持 Android 端下发的预格式化显示字段，C3 可直接显示路口/转向/距离。
- **iOS Watch 风格 UI 预留**：动画相位、大箭头、车道指引等显示逻辑已预留，真实屏幕实现时直接复用。
- **图标资源**：导航转向图标来自 [iconfont.cn](https://www.iconfont.cn/) 开源图标库。

## 目录结构

```
Auto/
├── platformio.ini          # 平台配置（ESP32-C3 / ESP32 / ESP32-S3）
├── README.md               # 本文件
└── src/
    ├── main.cpp            # 程序入口与模块组合
    ├── config/
    │   └── Config.h        # 常量、UUID、功能开关
    ├── ble/
    │   ├── BleClient.h     # BLE 客户端接口
    │   └── BleClient.cpp   # BLE 扫描/连接/订阅实现
    ├── nav/
    │   ├── NavData.h       # 导航数据结构
    │   ├── NavParser.h     # JSON 解析接口
    │   └── NavParser.cpp   # JSON 解析实现
    └── screen/
        ├── Screen.h                # 屏幕抽象接口
        ├── ScreenConsole.h         # 串口虚拟屏幕
        ├── ScreenConsole.cpp
        ├── ScreenRenderer.h        # 渲染辅助函数
        └── ScreenRenderer.cpp
```

## 快速开始

### 1. 安装 PlatformIO

- VS Code 安装 [PlatformIO IDE 插件](https://platformio.org/install/ide?install=vscode)。

### 2. 复制项目到本地

由于当前环境为 Linux 沙箱，项目已创建在 `/workspace/Auto`。请将该文件夹复制到你的 Windows 目录：

```
C:\Users\Axon\Documents\Code\Auto
```

### 3. 打开并编译

在 VS Code 中打开 `C:\Users\Axon\Documents\Code\Auto`，选择环境：

```ini
[env:esp32-c3-supermini]
```

点击 PlatformIO 的 **Build** 按钮，或执行：

```bash
pio run -e esp32-c3-supermini
```

### 4. 上传与查看串口

```bash
pio run -e esp32-c3-supermini --target upload
pio device monitor -e esp32-c3-supermini
```

## 硬件说明

- **默认板型**：`esp32-c3-devkitm-1`，适用于大多数 ESP32-C3 Super Mini 模块。
- 如果你的板子是 **Lolin C3 Mini**，请将 `platformio.ini` 中的 `board` 改为 `lolin_c3_mini`。

## 调试技巧

### 开关日志

编辑 `src/config/Config.h`：

```cpp
namespace Debug {
    constexpr bool LOG_BLE_RAW      = true;   // 原始 BLE JSON
    constexpr bool LOG_RENDER_STATE = true;   // 渲染状态
    constexpr bool LOG_ANIMATION    = true;   // 动画帧
    constexpr bool LOG_SYSTEM       = true;   // 系统日志
}
```

### 指定 Android MAC

在 `main.cpp` 中取消注释并填入真实 MAC，避免连到附近其他 BLE 设备：

```cpp
sBleClient.setTargetAddress("AA:BB:CC:DD:EE:FF");
```

### 配合 Android 简化模式

如果使用的是 **ESP32-C3 Super Mini** 等小内存/小 Flash 板，建议在 Android 端开启 **ESP32 简化模式**。此时固件会优先使用 Android 已经格式化好的字段：

- `turn_label`：转向简短标签（如“左转”）
- `distance_text`：路口距离文本（如“350m”）
- `intersection`：路口信息（如“当前路 → 下一道路”）

这些字段在 `NavParser` 中会自动解析；若未下发，固件也会使用本地 `ScreenRenderer` 进行转换。

### 模拟数据测试

可在 Android 端使用测试广播功能，或在 `setup()` 中手动构造 `Nav::NavState` 调用 `sScreen.setNavState()` 验证显示。

## 后续扩展

### 接入真实屏幕

1. 新建 `src/screen/ScreenOled.h` 和 `ScreenOled.cpp`，继承 `Screen`。
2. 在 `main.cpp` 中把 `ScreenConsole sScreen;` 替换为 `ScreenOled sScreen;`。
3. 复用 `ScreenRenderer` 中的标签和格式化函数。

### 屏幕 UI 设计建议（参考 iOS Watch 导航）

- **大箭头居中**：当前转向图标占据屏幕 50% 以上区域，使用粗描边圆角箭头。
- **距离数字突出**：路口剩余距离使用超大字体，位于箭头下方。
- **车道指引横条**：路口前显示多条车道，高亮推荐车道。
- **速度表盘**：右下角圆形表盘显示当前车速 / 限速。
- **脉冲动画**：接近路口时箭头做轻微缩放/脉冲动效。

### 协议瘦身

当前使用 JSON 便于调试。若数据量大或屏幕刷新要求高，可在 Android 端改为二进制 TLV 格式，ESP32 端解析更快。

## 使用 GitHub Actions / Codespaces 编译

### 在线编译（无需本地安装 PlatformIO）

#### 方式一：GitHub Codespaces

点击仓库首页的 **Code → Codespaces → Create codespace on main**，容器启动后执行：

```bash
pio run -e esp32-c3-supermini
```

Codespaces 已预装 PlatformIO CLI，编译完成后可在 `.pio/build/esp32-c3-supermini/firmware.bin` 下载固件。

#### 方式二：GitHub Actions

每次推送 `main` 或 `trae/solo-agent-*` 分支，以及提交 `v*` 标签时，Actions 会自动编译三种板型：

- `firmware-esp32-c3-supermini.bin`
- `firmware-esp32dev.bin`
- `firmware-esp32-s3-devkitc-1.bin`

编译产物可在：

- **Actions 页面** → 对应 workflow run → Artifacts
- **Releases 页面**（仅 tag 推送时自动创建）

### 直接刷入 Release 固件

1. 到 [Releases](https://github.com/BossforAlex/nav-ble-relay/releases) 下载对应板型的 `.bin`。
2. 使用 esptool：

```bash
esptool.py --chip esp32c3 --port COMx write_flash 0x0 firmware-esp32-c3-supermini.bin
```

3. 或使用 [ESPtool 在线工具](https://esphome.github.io/esp-web-tools/)、Thonny、PlatformIO 等烧录。

## 版本与分支说明

- `main`：稳定主线。
- `trae/solo-agent-*`：开发/实验分支。
- Tags `v*`：每次发布固件版本，Actions 会自动生成 Release 并上传二进制。

## 常见问题

**Q: 编译报错找不到 `ArduinoJson.h`**  
A: 确保 PlatformIO 已下载依赖，执行 `pio lib install` 或删除 `.pio/libdeps` 后重新编译。

**Q: C3 Super Mini 上传失败**  
A: 按住 BOOT 键再上电/复位进入下载模式；部分板子需选择正确的 `board` 定义。

**Q: 刷写固件后蓝牙无法启动或扫描不到设备**  
A: 请确认：
  1. `platformio.ini` 中的 `board` 与你的板型一致（C3 Super Mini 多数用 `esp32-c3-devkitm-1`，Lolin C3 Mini 用 `lolin_c3_mini`）。
  2. 首次烧录建议先执行 `pio run -e esp32-c3-supermini --target erase`，清除旧分区表。
  3. 确保天线附近没有强干扰，并在 Android 端确认 BLE 服务已启动。

**Q: 数据解析失败或中文道路名显示乱码**  
A: 打开 Android 端 **ESP32 简化模式**，只发送必要字段；同时检查固件日志中 `JsonDocument` 是否成功解析。

**Q: 扫描不到 Android 设备**  
A: 确保 Android 端 NavBleService 已启动，并且 BLE 广播中包含服务 UUID `0000FFE0-...`。
