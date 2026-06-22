# AutoNavDisplay

ESP32-C3 Super Mini 导航信息接收与显示项目（PlatformIO）。

## 项目特点

- **可移植**：通过 `Screen` 抽象层解耦显示驱动，当前用串口虚拟屏幕调试，后续可无缝替换为 OLED/LCD/TFT。
- **模块化**：BLE、协议解析、渲染逻辑分层清晰，便于单独测试和修改。
- **调试友好**：所有关键状态通过串口输出，无需屏幕即可验证数据链路。
- **iOS Watch 风格 UI 预留**：动画相位、大箭头、车道指引等显示逻辑已预留，真实屏幕实现时直接复用。

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

在 `main.cpp` 中取消注释并填入真实 MAC：

```cpp
sBleClient.setTargetAddress("AA:BB:CC:DD:EE:FF");
```

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

## 常见问题

**Q: 编译报错找不到 `ArduinoJson.h`**  
A: 确保 PlatformIO 已下载依赖，执行 `pio lib install` 或删除 `.pio/libdeps` 后重新编译。

**Q: C3 Super Mini 上传失败**  
A: 按住 BOOT 键再上电/复位进入下载模式；部分板子需选择正确的 `board` 定义。

**Q: 扫描不到 Android 设备**  
A: 确保 Android 端 NavBleService 已启动，并且 BLE 广播中包含服务 UUID `0000FFE0-...`。
