# Vibe Coding 上下文文档

本文档汇总本项目的核心上下文、设计规范与编程约定，供后续 AI / 开发者快速对齐。

## 1. 项目定位

- **名称**：NavBleRelay / AutoNavDisplay
- **功能**：Android 端读取高德地图车机版公开导航广播，通过 BLE 实时转发给 ESP32 显示。
- **原则**：无需高德 SDK，纯本地；BLE 外设模式；低内存 ESP32 友好。

## 2. 角色分工

| 端 | 角色 | 关键文件 |
|---|---|---|
| Android | BLE GATT Server / 外围设备 | `BleGattServer.kt`、`MainActivity.kt`、`NavBleService.kt` |
| ESP32 | BLE Central / 客户端 | `BleClient.cpp`、`NavParser.cpp`、`ScreenConsole.cpp` |

## 3. BLE 约定

- **服务 UUID**：`0000FFE0-0000-1000-8000-00805F9B34FB`
- **特征值 UUID**：FFE1(引导)、FFE2(车道)、FFE3(路况)、FFE4(状态)、FFE5(定位)
- **设备名前缀**：`ICA`
  - Android 端广播名固定为 `ICA`。
  - ESP32 本地名固定为 `ICA`。
  - 双方扫描/连接时仅识别名称以 `ICA` 开头的设备。
- **配对策略**：默认不绑定，靠名称 + 可选 MAC 白名单过滤。

## 4. 数据协议

- 所有数据以 JSON `notify` 发送。
- Android 为 ESP32-C3 等小内存设备提供“简化模式”，只发送转向、路口、距离等核心字段，并预格式化 `turn_label`、`distance_text`、`intersection`。
- ESP32 解析失败后需回退到本地拼接显示，不能崩溃。

## 5. UI 设计规范

- 采用 **Material 3 Expressive** 配色，支持浅色/深色模式。
- 主色调参考 `colors.xml`：Primary `#415F91`、Surface `#F9F9FF`、Error `#BA1A1A`。
- 卡片统一使用 `card_bg_dark`，圆角 16dp，带 1dp 描边。
- 工具栏图标使用 `md_on_surface_variant` 着色，保持 24dp 视觉大小。
- 导航转向图标来自 iconfont.cn 开源图标库。

## 6. ESP32 硬件约束

- 当前仅支持并发布 **ESP32-C3 Super Mini** 固件。
- Flash 使用 `DIO` 模式、`40MHz`、4MB。
- **ESP32-C3 内存布局**（与经典 ESP32 不同）：
  - `0x0000`  bootloader.bin
  - `0x8000`  partitions.bin
  - `0x10000` firmware.bin
- Release 固件为 `esptool merge_bin` 合并的工厂镜像，烧录地址 `0x0000`。
- 避免在 `setup()` 中阻塞，BLE 扫描延迟到 `loop()` 启动。
- `BLEDevice::setPower` 使用 `ESP_PWR_LVL_P7`，避免高功率在部分 C3 核心上导致异常。

## 7. 代码约定

- Kotlin：显式导入 `R`，不依赖隐式；日志统一走 `BleLogStore`。
- C++：ArduinoJson 7 默认构造 `JsonDocument doc;`；字符串拷贝注意截断；不使用带大小的构造。
- 资源：新增图标统一为 Vector Drawable，命名 `ic_*.xml`。
- 权限：Android S+ 使用 `BLUETOOTH_CONNECT` / `BLUETOOTH_ADVERTISE`。

## 8. CI / Release

- GitHub Actions 构建：仅 `esp32-c3-supermini` 固件 + `app-debug.apk`。
- Release 产物：`firmware-esp32-c3-supermini.bin`、`app-debug.apk`，源码由 GitHub 自动生成。
- 不再发布 ESP32-DevKit / ESP32-S3 固件。
- 固件产物通过 `esptool.py merge_bin` 合并 bootloader、partitions、app 为单文件。

## 9. 最近关键决策

- v0.2.x 起统一设备名前缀为 `ICA`。
- 固件改为工厂镜像，解决手机 ESPFlasher / flash_download_tool 烧录地址错误问题。
- 蓝牙日志页使用独立图标 `ic_ble_logs` 与 `ic_clear`，条目样式与主界面卡片保持一致。
