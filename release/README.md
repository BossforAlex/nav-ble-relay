# ESP32-S3/C3 AutoNavDisplay v0.5.5 — BLE 串口 spam 修复

## ⚠️ 云端编译无法完成 — 需要您本地编译

### 沙盒限制结论
我已确认当前云端沙盒的出站策略**禁止下载 ESP32 toolchain**（约 100MB+ 的预编译 gcc 工具链）：

- `dl.registry.platformio.org` (PlatformIO CDN) GET 返回 404（沙盒缓存与 CDN 实际状态不一致）
- `dl.espressif.com` → 302 → `dl.espressif.cn` 仍 404
- `github.com/espressif/crosstool-NG/releases` 仅有源码（`*-src.tar.gz`），没有预编译二进制

小文件（≤10MB）下载正常；toolchain/framework 类大文件全被沙盒的 `127.0.0.1:18080` 透明代理切断。
所以**云端无法产出 release 固件**。

### 已完成：所有源码修改
`/workspace/release/v0.5.5-fix-nimble-serial-spam.patch` 包含全部 5 个文件的修改：

| 文件 | 修改内容 |
|---|---|
| `Auto/platformio.ini` | `CORE_DEBUG_LEVEL 3→1`、新增 `CONFIG_BT_NIMBLE_LOG_LEVEL=1`、平台 pin 到 `espressif32@6.5.0` |
| `Auto/src/ble/BleServer.cpp` | `begin()` 顶部新增 12 行 `esp_log_level_set(...)` 关闭 NimBLE 全部内部 INFO 日志 + 全局兜底 `esp_log_level_set("*", ESP_LOG_WARN)` |
| `Auto/src/config/Config.h` | 版本号 `0.5.4 → 0.5.5` |
| `Auto/src/main.cpp` | 启动横幅改用 `PROJECT_VERSION` 宏（去掉硬编码 v0.5.4） |
| `Auto/src/screen/ScreenConsole.cpp` | 移除 `setBleConnected` 内重复的 `[BLE] 状态变化: 已连接/已断开`（BleServer.loop 已输出更精准版本） |

### 您本地的编译步骤（一次性，3-5 分钟）

```bash
# 1. 把 patch 应用到您的项目
cd /path/to/your/nav-ble-relay
git apply v0.5.5-fix-nimble-serial-spam.patch
# 或者直接对照 diff 自己改

# 2. 编译 S3 主力环境
cd Auto
pio run -e esp32-s3-supermini

# 3. 编译 C3 后备环境
pio run -e esp32-c3-supermini

# 4. 烧录
pio run -e esp32-s3-supermini -t upload
# 或手动：把 .pio/build/esp32-s3-supermini/firmware.bin 用 esptool.py 烧入 0x10000
```

### 验证是否生效
烧录后串口应只看到：

```
██  AutoNavDisplay v0.5.5
██  FW: v0.5.5  (Jul  7 2026 06:00:00)
...
[BLE] ✓ 等待手机连接...
// 之后手机连接时仅看到 1 行
[BLE] ✓ 手机已连接（连接数=1 MTU=517）
// 不应再出现 "NimBLEServer: subscribe event" 或 "mtu update event" 刷屏
```

### 关键修复原理
1. **`CONFIG_BT_NIMBLE_LOG_LEVEL=1`（编译期）**：NimBLE-Arduino 的 `NIMBLE_LOGI` 宏在 level≥2 时输出，level=1 时所有 `LOGI` 调用都被 `#define` 为 noop——**从根源消灭 subscribe/mtu 等 INFO 日志**。
2. **`esp_log_level_set("NimBLE*", ESP_LOG_NONE)`（运行期）**：兜底，防止 Arduino 内核或 NimBLE 内部某处把 log level 重新设回 INFO。
3. **`esp_log_level_set("*", ESP_LOG_WARN)`**：全局兜底。
4. **ScreenConsole 去重**：BleServer.loop 已经输出更精确的连接提示，ScreenConsole 重复打印是无意义的。

### 沙盒已验证事项
- ✅ PlatformIO Core 6.1.19 已安装
- ✅ espressif32@6.5.0 平台源码下载成功
- ❌ espressif/toolchain-xtensa-esp32s3 下载失败（沙盒网络限制）
- ❌ 完整编译未完成

### 如果您有更宽松的出站权限的沙盒
请运行 `pio pkg install -g -p "platformio/espressif32@6.5.0"` 单独验证 toolchain 是否能下到。
如果能下，整套编译会自动完成。
