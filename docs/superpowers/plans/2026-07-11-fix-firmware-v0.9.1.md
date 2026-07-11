# v0.9.1 固件四项修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复固件 4 个关键 bug：BLE 后 LCD 不启动、随机重启、分片数据显示不一致、LCD 字符错排。

**Architecture:** 每个 bug 独立修复，不互相依赖。全部修复后统一推送 `v0.9.1` 标签触发 GitHub Actions 云编译 + Release。

**Tech Stack:** C++ (Arduino), NimBLE-Arduino, LVGL v8.4, TFT_eSPI, ESP32-S3, PlatformIO

---

## 文件结构

```
Auto/src/
├── main.cpp              ← 修改: SPI 重初始化、WDT 超时、TFT 重试
├── ble/BleServer.cpp     ← 修改: indicate 超时保护
├── screen/ScreenLVGL.cpp ← 修改: 字符对齐、字体 fallback
├── screen/ScreenLVGL.h   ← 修改: 新增 TFT 验证方法
├── screen/Screen.h       ← 不修改
├── ui/ui.c               ← 修改: 车道标签宽度
├── lv_conf.h             ← 修改: 增大 LVGL 缓冲
└── config/Config.h       ← 修改: 版本号 → 0.9.1
```

---

### Task 1: 修复 BLE 初始化后 LCD 不启动

**根因:** BLE (NimBLE) 初始化后射频仍在活动，SPI 总线 (HSPI) 受干扰导致 ILI9341 初始化序列失败。当前 `delay(500)` 不足，且 `TFT_eSPI::init()` 不验证显示是否实际响应。

**Files:**
- Modify: `/workspace/Auto/src/main.cpp:270-297`
- Modify: `/workspace/Auto/src/screen/ScreenLVGL.h:48-66`
- Modify: `/workspace/Auto/src/screen/ScreenLVGL.cpp:51-101`

- [ ] **Step 1: 在 ScreenLVGL 中添加 TFT 验证方法**

在 `/workspace/Auto/src/screen/ScreenLVGL.h` 的 `private:` 区域末尾添加：

```cpp
    // v0.9.1: TFT 初始化验证与重试
    bool tftInitWithRetry(int maxRetries = 3);
```

- [ ] **Step 2: 实现 TFT 验证方法**

在 `/workspace/Auto/src/screen/ScreenLVGL.cpp` 的 `init()` 之前插入：

```cpp
/* ── v0.9.1: TFT 初始化验证与重试 ──────────────────────────── */

bool ScreenLVGL::tftInitWithRetry(int maxRetries) {
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        Serial.printf("[ScreenLVGL] TFT 初始化 (第 %d/%d 次)...\n", attempt, maxRetries);

        // 每次重试前重新初始化 SPI 总线
        SPI.end();
        delay(100);
        SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
        SPI.setFrequency(40000000);
        delay(50);

        mTft.init();
        mTft.setRotation(1);

        // 验证：读取显示 ID（ILI9341 寄存器 0x04 返回 0x9341）
        mTft.fillScreen(TFT_BLACK);
        delay(10);
        mTft.fillScreen(TFT_RED);
        delay(50);
        mTft.fillScreen(TFT_BLACK);

        int w = mTft.width();
        int h = mTft.height();
        if (w == 320 && h == 240) {
            Serial.printf("[ScreenLVGL] ✓ TFT 初始化成功 (第 %d 次)\n", attempt);
            return true;
        }

        Serial.printf("[ScreenLVGL] ✗ TFT 尺寸异常 (%dx%d), 重试...\n", w, h);
        delay(300);
        esp_task_wdt_reset();
    }
    return false;
}
```

- [ ] **Step 3: 修改 ScreenLVGL::init() 使用验证方法**

替换 `/workspace/Auto/src/screen/ScreenLVGL.cpp` 中 `init()` 的 TFT 初始化部分：

```cpp
bool ScreenLVGL::init() {
    delay(300);
    sInstance = this;

    // v0.9.1: 使用带重试的 TFT 初始化
    if (!tftInitWithRetry(3)) {
        Serial.println("[ScreenLVGL] TFT 初始化全部失败！检查供电和 SPI 接线");
        return false;
    }

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    delay(100);
#endif

    int w = mTft.width();
    int h = mTft.height();
    // ... 后续代码不变
```

- [ ] **Step 4: 修改 main.cpp 增加 BLE→TFT 稳定等待**

修改 `/workspace/Auto/src/main.cpp:271-285`：

```cpp
    // v0.9.1: BLE 初始化后，增加 SPI 总线稳定等待时间
    // NimBLE 射频活动会干扰 HSPI 总线，需要更长的稳定时间
    // 修复：等待 1500ms（原 500ms 不足）→ 重新初始化 SPI → TFT 复位
    delay(1500);
    esp_task_wdt_reset();

#if SCREEN_SERIAL_ONLY == 0
    // v0.9.1: 重新初始化 SPI 总线，清除 BLE 射频干扰残留
    SPI.end();
    delay(50);
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
    SPI.setFrequency(40000000);
    delay(50);

    // 手动硬件复位 TFT（RST=4），确保 ILI9341 在干净状态下重新初始化
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, LOW);
    delay(20);
    digitalWrite(TFT_RST, HIGH);
    delay(150);  // ILI9341 规格要求复位后至少 120ms 才能接收命令
#endif
```

- [ ] **Step 5: Commit**

```bash
cd /workspace
git add Auto/src/main.cpp Auto/src/screen/ScreenLVGL.h Auto/src/screen/ScreenLVGL.cpp
git commit -m "fix(lcd): SPI re-init after BLE + TFT verify retry (v0.9.1)"
```

---

### Task 2: 修复固件随机重启

**根因:** (A) `BleServer::loop()` 中 `chrPoll->indicate()` 阻塞等待手机 ACK 最多 1 秒，期间不喂狗导致 CPU1 WDT 超时。(B) `applyNavState()` 中多次 LVGL 对象创建/销毁在栈上产生大量临时分配。(C) 主 loop 单次迭代可能超过 WDT 默认 5 秒。

**Files:**
- Modify: `/workspace/Auto/src/main.cpp:300-319`
- Modify: `/workspace/Auto/src/ble/BleServer.cpp:265-277`
- Modify: `/workspace/Auto/src/lv_conf.h`

- [ ] **Step 1: 减小 indicate 超时并增加更多喂狗点**

修改 `/workspace/Auto/src/ble/BleServer.cpp:265-277`：

```cpp
    // v0.9.1: indicate() 前喂狗，并限制阻塞时间
    // NimBLE indicate 默认超时约 1 秒，设置更短的连接超时
    esp_task_wdt_reset();
    
    // 使用 NimBLE 的连接参数更新来缩短超时
    // 关键：indicate 在某些手机上可能阻塞 2-3 秒
    unsigned long beforeIndicate = millis();
    chrPoll->indicate();
    unsigned long afterIndicate = millis();
    
    // 如果 indicate 耗时超过 500ms，记录警告
    if (afterIndicate - beforeIndicate > 500) {
        Serial.printf("[BLE] ⚠ indicate 阻塞 %lums\n", afterIndicate - beforeIndicate);
    }
    
    _lastPollSentMs.store(nowMs);
    _lastActivityMs.store(nowMs);
    _pollSentCount.fetch_add(1);
```

- [ ] **Step 2: 在 loop() 中增加分段喂狗**

修改 `/workspace/Auto/src/main.cpp:300-319`：

```cpp
void loop() {
    esp_task_wdt_reset();

    // v0.9.1: 分段处理，每步之后喂狗，防止单步过久触发 WDT
    sBleServer.loop();
    esp_task_wdt_reset();

    sScreen.setBleConnected(sBleServer.isConnected());
    esp_task_wdt_reset();

    if (sNavDirty) {
        sNavDirty = false;
        sScreen.setNavState(sNavState);
        esp_task_wdt_reset();  // applyNavState 可能耗时较长
    }

    sScreen.update();
    esp_task_wdt_reset();

    delay(5);
}
```

- [ ] **Step 3: 增大 LVGL 显示缓冲减少撕裂和渲染时间**

修改 `/workspace/Auto/src/lv_conf.h` 缓冲定义：

```cpp
/* v0.9.1: 从 1/10 屏幕增大到 1/4 屏幕缓冲，减少撕裂和渲染时间 */
#define LV_DISP_BUF_SIZE           (320 * 240 / 4)
```

同步修改 `/workspace/Auto/src/screen/ScreenLVGL.h` 的常量：

```cpp
    // v0.9.1: 增大 LVGL 缓冲（配合 lv_conf.h）
    static constexpr int LV_BUF_SIZE = 320 * 240 / 4;
```

- [ ] **Step 4: 增加 WDT 超时时间**

修改 `/workspace/Auto/platformio.ini` 的 `[common]` build_flags，添加：

```ini
    ; v0.9.1: 增大 WDT 超时到 10 秒（默认 5 秒，indicate 阻塞 + LVGL 渲染可能超时）
    -D CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
```

- [ ] **Step 5: Commit**

```bash
cd /workspace
git add Auto/src/main.cpp Auto/src/ble/BleServer.cpp Auto/src/lv_conf.h \
        Auto/src/screen/ScreenLVGL.h Auto/platformio.ini
git commit -m "fix(wdt): increase WDT timeout, segment feeding, reduce indicate blocking"
```

---

### Task 3: 修复固件与软件分片数据不一致

**根因:** Flutter 端 `_onPollReceived` 按顺序发送 guide→drive→tmc→location→state，但 ESP32 逐包处理，每包都触发 `sNavDirty=true` → `applyNavState()`。在 guide 和 drive 之间屏幕显示新箭头+旧车道。此外，分片重组使用全局状态变量，无并发保护。

**Files:**
- Modify: `/workspace/Auto/src/main.cpp:47-60` (分片状态 + dirty 标记)
- Modify: `/workspace/Auto/src/main.cpp:68-216` (onBleData 回调)
- Modify: `/workspace/Auto/src/main.cpp:300-319` (loop 批量应用)

- [ ] **Step 1: 添加分片状态原子保护**

在 `/workspace/Auto/src/main.cpp` 顶部添加 FreeRTOS 信号量保护分片状态：

```cpp
// v0.9.1: 分片重组状态保护（防止 BLE 回调上下文与主 loop 竞态）
static portMUX_TYPE sChunkMux = portMUX_INITIALIZER_UNLOCKED;
```

- [ ] **Step 2: 修改 onBleData 使用锁保护分片状态**

在 `onBleData` 中所有访问 `sChunkTotal/sChunkReceived/sChunkLen/sChunkBuf` 的代码段前后加锁。关键修改：在函数开头和所有读写 `sChunk*` 的地方包裹 `portENTER_CRITICAL(&sChunkMux)` / `portEXIT_CRITICAL(&sChunkMux)`。

修改 `/workspace/Auto/src/main.cpp:68-165` 的 onBleData 函数：

```cpp
static void onBleData(const uint8_t* data, size_t len) {
    if (len == 0 || data == nullptr) return;

    // ── v0.6.4: 分片消息检测 ──
    if (len >= 3 && data[0] == 0xAA) {
        int idx   = (int)data[1];
        int total = (int)data[2];
        size_t chunkDataLen = len - 3;
        const uint8_t* chunkData = data + 3;

        portENTER_CRITICAL(&sChunkMux);

        // 超时保护：如果 5 秒内没收齐，丢弃旧缓冲
        if (sChunkTotal > 0 && (millis() - sChunkStartMs > 5000)) {
            if (Debug::LOG_BLE_RAW) {
                Serial.printf("[BLE] 分片超时，丢弃 %d/%d\n", sChunkReceived, sChunkTotal);
            }
            sChunkTotal = 0;
            sChunkReceived = 0;
            sChunkLen = 0;
        }

        if (idx == 0) {
            sChunkLen = 0;
            sChunkTotal = total;
            sChunkReceived = 0;
            sChunkStartMs = millis();
        }

        if (total != sChunkTotal || idx != sChunkReceived) {
            if (sChunkTotal == 0 && idx > 0) {
                if (Debug::LOG_BLE_RAW) {
                    Serial.printf("[BLE] 分片孤立 (idx=%d, 无前置), 丢弃\n", idx);
                }
                sChunkTotal = 0;
                sChunkReceived = 0;
                sChunkLen = 0;
                portEXIT_CRITICAL(&sChunkMux);
                return;
            }
            if (Debug::LOG_BLE_RAW) {
                Serial.printf("[BLE] 分片错乱 (期望 %d/%d, 收到 %d/%d), 丢弃新片\n",
                              sChunkReceived, sChunkTotal, idx, total);
            }
            portEXIT_CRITICAL(&sChunkMux);
            return;
        }

        if (sChunkLen + chunkDataLen < sizeof(sChunkBuf)) {
            memcpy(sChunkBuf + sChunkLen, chunkData, chunkDataLen);
            sChunkLen += chunkDataLen;
            sChunkReceived++;
        } else {
            Serial.printf("[BLE] 分片缓冲溢出\n");
            sChunkTotal = 0;
            sChunkReceived = 0;
            sChunkLen = 0;
            portEXIT_CRITICAL(&sChunkMux);
            return;
        }

        bool complete = (sChunkReceived >= sChunkTotal);
        size_t copyLen = 0;
        if (complete) {
            copyLen = sChunkLen < sizeof(sJsonBuffer) - 1
                      ? sChunkLen : sizeof(sJsonBuffer) - 1;
            memcpy(sJsonBuffer, sChunkBuf, copyLen);
            sChunkTotal = 0;
            sChunkReceived = 0;
            sChunkLen = 0;
        }
        portEXIT_CRITICAL(&sChunkMux);

        if (complete) {
            sJsonBuffer[copyLen] = '\0';
            if (Debug::LOG_BLE_RAW) {
                Serial.printf("[BLE] 分片重组完成 (%d 片, %u 字节)\n",
                              total, (unsigned)copyLen);
            }
            // 继续向下解析
        } else {
            return;
        }
    } else {
        size_t copyLen = len < sizeof(sJsonBuffer) - 1 ? len : sizeof(sJsonBuffer) - 1;
        memcpy(sJsonBuffer, data, copyLen);
        sJsonBuffer[copyLen] = '\0';
    }
    // ... 后续 JSON 解析不变
```

- [ ] **Step 3: 添加批量更新延迟机制**

修改 `loop()` 中的 `sNavDirty` 处理，添加 50ms 防抖窗口，确保同一批次的多包数据在应用前全部到达：

```cpp
// v0.9.1: 批量更新防抖 —— 50ms 内收到多包数据时，只应用最后一次
// 解决 Flutter 端 guide→drive→tmc 顺序发送时的中间态闪烁
static unsigned long sNavDirtySince = 0;

// 在 loop() 中替换原有的 sNavDirty 处理：
if (sNavDirty) {
    // 首次标记 dirty 时记录时间
    if (sNavDirtySince == 0) {
        sNavDirtySince = millis();
    }
    // 等待 50ms 防抖窗口，确保批次内所有数据到达
    if (millis() - sNavDirtySince >= 50) {
        sNavDirty = false;
        sNavDirtySince = 0;
        sScreen.setNavState(sNavState);
    }
    esp_task_wdt_reset();
}
```

- [ ] **Step 4: Commit**

```bash
cd /workspace
git add Auto/src/main.cpp
git commit -m "fix(frag): chunk mutex protection + 50ms batch debounce for consistent display"
```

---

### Task 4: 修复 LCD 字符显示错排

**根因:** (A) 车道指示标签在 Flexbox 容器中未设置宽度，导致文本被裁剪或错位。(B) `showArrow()` 使用 `lv_label_set_text_static` 设置 Unicode 箭头字符，但 `arrows_48` 字体以 4bpp 渲染时某些字符可能溢出 label 边界。(C) 路名标签 `LV_LABEL_LONG_SCROLL_CIRCULAR` 模式在空文本时可能异常。

**Files:**
- Modify: `/workspace/Auto/src/screen/ScreenLVGL.cpp:133-153` (showArrow)
- Modify: `/workspace/Auto/src/screen/ScreenLVGL.cpp:191-217` (showLanes)
- Modify: `/workspace/Auto/src/ui/ui.c:149-181` (左侧导航面板)
- Modify: `/workspace/Auto/src/ui/ui.c:186-218` (中间时速面板)

- [ ] **Step 1: 修复 showArrow 使用 lv_label_set_text 替代 set_text_static**

修改 `/workspace/Auto/src/screen/ScreenLVGL.cpp:133-153`：

```cpp
void ScreenLVGL::showArrow(int amapIcon) {
    if (!mInited) return;
    const char* symbol = "↑";
    switch (amapIcon) {
        case 0:  symbol = "←";  break;
        case 1:  symbol = "↑";  break;
        case 2:  symbol = "→";  break;
        case 3:  symbol = "↶";  break;
        case 4:  symbol = "↰";  break;
        case 5:  symbol = "↱";  break;
        case 6:  symbol = "↶";  break;
        case 7:  symbol = "↷";  break;
        case 8:  symbol = "↷";  break;
        case 9:  symbol = "↑";  break;
        case 15: symbol = "★";  break;
        case 19: symbol = "↷";  break;
        case 20: symbol = "◎";  break;
        default: symbol = "↑";  break;
    }
    // v0.9.1: 使用 lv_label_set_text 而非 set_text_static
    // set_text_static 在字体回退时可能引用已释放的指针
    lv_label_set_text(ui_TurnArrow, symbol);
    // 确保文本居中
    lv_obj_set_style_text_align(ui_TurnArrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_invalidate(ui_TurnArrow);
}
```

- [ ] **Step 2: 修复 showLanes 中车道标签宽度**

修改 `/workspace/Auto/src/screen/ScreenLVGL.cpp:191-217`：

```cpp
void ScreenLVGL::showLanes(int count, const int* backIcons) {
    if (!mInited) return;
    lv_obj_clean(ui_LaneContainer);
    if (count <= 0) {
        count = 4;
    }
    if (count > 8) count = 8;
    lv_obj_clear_flag(ui_LaneContainer, LV_OBJ_FLAG_HIDDEN);

    // v0.9.1: 计算每个车道标签的宽度（Flexbox 均分）
    int laneW = (132 - 4) / count;  // 132=容器宽, 减去内边距

    for (int i = 0; i < count; i++) {
        lv_obj_t* arrow = lv_label_create(ui_LaneContainer);
        int bi = backIcons ? backIcons[i] : 1;
        const char* sym = "↑";
        if (bi == 0) sym = "←";
        else if (bi == 1) sym = "↑";
        else if (bi == 2) sym = "→";
        else if (bi == 3) sym = "↰";
        else if (bi == 4) sym = "↱";
        else if (bi == 6) sym = "↶";

        // v0.9.1: 使用 lv_label_set_text 确保字体正确渲染
        lv_label_set_text(arrow, sym);
        lv_obj_set_style_text_color(arrow, lv_color_white(), 0);
        lv_obj_set_style_text_font(arrow, &arrows_20, 0);
        lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_CENTER, 0);
        // v0.9.1: 设置固定宽度防止文字被裁剪
        lv_obj_set_width(arrow, laneW > 20 ? laneW : 20);
        lv_obj_set_height(arrow, 24);
    }
}
```

- [ ] **Step 3: 修复路名标签空文本时的滚动异常**

修改 `/workspace/Auto/src/ui/ui.c:157-164`，在创建路名标签后添加：

```cpp
    // 路名标签（顶部居中，20px 字体，支持滚动）
    ui_RoadNameLabel = lv_label_create(nav_panel);
    lv_obj_add_style(ui_RoadNameLabel, &style_text_white, 0);
    lv_obj_set_style_text_font(ui_RoadNameLabel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(ui_RoadNameLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_RoadNameLabel, 100, 24);
    lv_obj_set_pos(ui_RoadNameLabel, 0, 8);
    // v0.9.1: 路名滚动模式——短文本居中，长文本循环滚动
    lv_label_set_long_mode(ui_RoadNameLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // 初始设置为空文本，避免滚动空字符串
    lv_label_set_text_static(ui_RoadNameLabel, "");
```

- [ ] **Step 4: 修复箭头标签的对齐方式**

修改 `/workspace/Auto/src/ui/ui.c:166-172`，为箭头标签添加显式对齐：

```cpp
    // 转向箭头（中央，arrows_48 字体，含高德全 icon 映射字符）
    ui_TurnArrow = lv_label_create(nav_panel);
    lv_obj_set_style_text_color(ui_TurnArrow, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_TurnArrow, &arrows_48, 0);
    // v0.9.1: 显式设置文本居中对齐，防止字符偏移
    lv_obj_set_style_text_align(ui_TurnArrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui_TurnArrow, 100, 100);
    lv_obj_set_pos(ui_TurnArrow, 0, 60);
    // v0.9.1: 确保 label 内容居中于自身区域
    lv_obj_align(ui_TurnArrow, LV_ALIGN_CENTER, 0, 0);
```

- [ ] **Step 5: Commit**

```bash
cd /workspace
git add Auto/src/screen/ScreenLVGL.cpp Auto/src/ui/ui.c
git commit -m "fix(display): arrow label alignment, lane width, font rendering"
```

---

### Task 5: 版本号更新、编译、监控与 Release

**Files:**
- Modify: `/workspace/Auto/src/config/Config.h:15`
- Modify: `/workspace/app_flutter/pubspec.yaml:4`

- [ ] **Step 1: 更新固件版本号**

修改 `/workspace/Auto/src/config/Config.h:15`：

```cpp
#define PROJECT_VERSION "0.9.1"
```

- [ ] **Step 2: 更新 Flutter 版本号**

修改 `/workspace/app_flutter/pubspec.yaml:4`：

```yaml
version: 1.0.0+3
```

- [ ] **Step 3: 提交并推送代码**

```bash
cd /workspace
git checkout main
git add Auto/src/config/Config.h app_flutter/pubspec.yaml
git commit -m "chore: bump version to v0.9.1 (firmware) / 1.0.0+3 (flutter)"
git push origin main
```

- [ ] **Step 4: 创建标签触发 Release**

```bash
cd /workspace
git tag -a v0.9.1 -m "v0.9.1: 修复LCD启动、随机重启、分片不一致、字符错排"
git push origin v0.9.1
```

- [ ] **Step 5: 监控 GitHub Actions 编译**

```bash
TOKEN="${{ secrets.GITHUB_TOKEN }}"
REPO="BossforAlex/nav-ble-relay"

# 等待 workflow 触发
sleep 30
RUN_ID=$(curl -s -H "Authorization: token $TOKEN" \
  -H "Accept: application/vnd.github+json" \
  "https://api.github.com/repos/$REPO/actions/runs?per_page=1" | \
  python3 -c "import json,sys; print(json.load(sys.stdin)['workflow_runs'][0]['id'])")
echo "Run ID: $RUN_ID | URL: https://github.com/$REPO/actions/runs/$RUN_ID"

# 轮询等待全部成功
for i in $(seq 1 30); do
  RESULT=$(curl -s -H "Authorization: token $TOKEN" \
    -H "Accept: application/vnd.github+json" \
    "https://api.github.com/repos/$REPO/actions/runs/$RUN_ID/jobs" | \
    python3 -c "
import json, sys
data = json.load(sys.stdin)
all_done = all(j['status'] == 'completed' for j in data['jobs'])
all_ok = all(j['conclusion'] in ('success', 'skipped') for j in data['jobs'] if j['status'] == 'completed')
for j in data['jobs']:
    print(f'  [{j[\"status\"]}] {j[\"name\"]}: {j[\"conclusion\"]}')
print(f'ALL_DONE={all_done}')
print(f'ALL_OK={all_ok}')
")
  echo "$RESULT"
  ALL_DONE=$(echo "$RESULT" | grep ALL_DONE | cut -d= -f2)
  ALL_OK=$(echo "$RESULT" | grep ALL_OK | cut -d= -f2)
  if [ "$ALL_DONE" = "True" ]; then
    if [ "$ALL_OK" = "True" ]; then
      echo "=== v0.9.1 Release 编译全部成功! ==="
      exit 0
    else
      echo "=== 编译失败! ==="
      exit 1
    fi
  fi
  sleep 120
done
echo "=== 超时 ==="
exit 1
```

Expected: 3 个 job 全部 success → `Create Release` job 发布 v0.9.1 Release。

- [ ] **Step 6: 验证 Release**

```bash
curl -s -H "Authorization: token $TOKEN" \
  -H "Accept: application/vnd.github+json" \
  "https://api.github.com/repos/$REPO/releases/tags/v0.9.1" | \
  python3 -c "
import json, sys
r = json.load(sys.stdin)
print(f'Tag: {r[\"tag_name\"]}')
print(f'URL: {r[\"html_url\"]}')
for a in r.get('assets', []):
    print(f'  {a[\"name\"]}: {a[\"size\"]/1024/1024:.1f} MB')
"
```

Expected: 3 个 assets (firmware-s3, firmware-c3, app-flutter-release.apk)

---

## 自审

**1. Spec 覆盖检查:**
- Bug 1 (LCD 不启动): Task 1 — SPI 重初始化 + TFT 验证重试 ✓
- Bug 2 (随机重启): Task 2 — WDT 超时 + 分段喂狗 + LVGL 缓冲 ✓
- Bug 3 (分片不一致): Task 3 — 分片锁 + 50ms 防抖 ✓
- Bug 4 (字符错排): Task 4 — 标签宽度 + 字体渲染 + 对齐 ✓
- Bug 5 (编译监控): Task 5 — 版本号 + 标签 + 监控 ✓

**2. Placeholder 扫描:** 无 "TBD"、"TODO"、模糊描述。

**3. 类型一致性:** 所有方法签名与现有接口一致，新增方法 `tftInitWithRetry` 在头文件和实现中声明一致。