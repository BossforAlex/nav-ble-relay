# 导航BLE转发 (nav-ble-relay) — Code Wiki

> 最后更新: 2026-06-06

---

## 目录

1. [项目概述](#1-项目概述)
2. [项目整体架构](#2-项目整体架构)
3. [技术栈与依赖](#3-技术栈与依赖)
4. [模块详细说明](#4-模块详细说明)
   - [4.1 NavBleApp — 应用入口](#41-navbleapp--应用入口)
   - [4.2 MainActivity — 主界面](#42-mainactivity--主界面)
   - [4.3 NavBleService — 前台服务](#43-navbleservice--前台服务)
   - [4.4 NavBroadcastReceiver — 广播接收器](#44-navbroadcastreceiver--广播接收器)
   - [4.5 AmapAutoProtocol — 协议定义层](#45-amapautoprotocol--协议定义层)
   - [4.6 BleGattServer — BLE 通信层](#46-blegattserver--ble-通信层)
5. [数据流与交互流程](#5-数据流与交互流程)
6. [BLE 服务与特征值设计](#6-ble-服务与特征值设计)
7. [权限与安全](#7-权限与安全)
8. [项目运行方式](#8-项目运行方式)
9. [构建与发布](#9-构建与发布)
10. [目录结构总览](#10-目录结构总览)

---

## 1. 项目概述

**nav-ble-relay** 是一款 Android 应用，核心功能是将高德地图车机版（AmapAuto）发出的导航广播数据，通过 **蓝牙低功耗（BLE）** 实时转发给外部硬件设备（ESP32）。Android 设备充当 **BLE 外设（Peripheral/GATT Server）**，ESP32 作为 **中心设备（Central/GATT Client）** 主动连接并接收导航数据。

| 属性 | 值 |
|------|-----|
| 项目名称 | nav-ble-relay |
| 应用名称 | 导航BLE转发 |
| 包名 | `com.navblerelay` |
| 语言 | Kotlin |
| 最低 SDK | API 21 (Android 5.0) |
| 目标 SDK | API 35 (Android 15) |
| 编译 SDK | API 35 |
| 版本 | 1.0.0 (versionCode=1) |

---

## 2. 项目整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          Android 设备                                │
│                                                                     │
│  ┌──────────────┐     ┌──────────────────┐     ┌────────────────┐  │
│  │ 高德地图车机版 │────▶│NavBroadcastReceiver│────▶│  NavBleService  │  │
│  │  (AmapAuto)   │     │  (广播接收器)       │     │  (前台Service)   │  │
│  │              │     │                    │     │                │  │
│  │ 发送广播:     │     │ 解析 AmapAuto     │     │ 协调广播接收    │  │
│  │ ACTION_SEND  │     │ 标准广播协议       │     │ 与BLE转发       │  │
│  └──────────────┘     └──────────────────┘     └───────┬────────┘  │
│                                                        │           │
│                                               ┌────────▼────────┐  │
│                                               │  BleGattServer   │  │
│                                               │  (BLE GATT服务)   │  │
│                                               │                  │  │
│                                               │ 5个特征值通道    │  │
│                                               │ JSON序列化发送   │  │
│                                               └────────┬────────┘  │
│                                                        │           │
│  ┌──────────────┐                             BLE Notify │           │
│  │  MainActivity │                                        │           │
│  │  (启动/停止UI) │                                        │           │
│  └──────────────┘                                        │           │
└─────────────────────────────────────────────────────────┼───────────┘
                                                          │
                                                          ▼
                                               ┌──────────────────┐
                                               │   ESP32 设备      │
                                               │  (BLE Central)    │
                                               │                   │
                                               │ 订阅特征值通知    │
                                               │ 接收导航数据      │
                                               └──────────────────┘
```

### 分层架构

```
┌──────────────────────────────────────────────┐
│                UI 层 (Presentation)           │
│  MainActivity — 启动/停止服务、状态显示        │
├──────────────────────────────────────────────┤
│              服务层 (Service)                 │
│  NavBleService — 前台服务，生命周期管理        │
│  NavBleApp — Application 初始化               │
├──────────────────────────────────────────────┤
│             协议层 (Protocol)                 │
│  AmapAutoProtocol — 协议常量与映射表           │
│  GuideInfo / DriveWayInfo / TmcSegmentInfo   │
│  / LocationInfo / BleDataPacket — 数据模型    │
├──────────────────────────────────────────────┤
│             通信层 (Communication)            │
│  NavBroadcastReceiver — 广播接收与解析         │
│  BleGattServer — BLE GATT 服务与数据发送       │
└──────────────────────────────────────────────┘
```

---

## 3. 技术栈与依赖

### 构建工具链

| 组件 | 版本 |
|------|------|
| Android Gradle Plugin | 8.7.3 |
| Kotlin | 2.0.21 |
| Gradle | Wrapper (项目自带) |
| JVM Target | 17 |

### 运行时依赖

| 依赖 | 版本 | 用途 |
|------|------|------|
| `androidx.core:core-ktx` | 1.15.0 | Kotlin 扩展 API |
| `androidx.appcompat:appcompat` | 1.7.0 | 向后兼容的 Activity |
| `com.google.android.material:material` | 1.12.0 | Material Design 组件 |

### 仓库

- Maven Central
- Google Maven
- Gradle Plugin Portal

---

## 4. 模块详细说明

### 4.1 NavBleApp — 应用入口

**文件:** [NavBleApp.kt](file:///workspace/app/src/main/java/com/navblerelay/NavBleApp.kt)

**职责:** 自定义 Application 类，在 `AndroidManifest.xml` 中注册为 `android:name=".NavBleApp"`。

**关键方法:**

| 方法 | 说明 |
|------|------|
| `onCreate()` | 应用初始化时调用，记录日志 |

**要点:**
- 继承 `Application()`
- 当前仅做日志输出，为后续全局初始化预留扩展点

---

### 4.2 MainActivity — 主界面

**文件:** [MainActivity.kt](file:///workspace/app/src/main/java/com/navblerelay/MainActivity.kt)

**布局:** [activity_main.xml](file:///workspace/app/src/main/res/layout/activity_main.xml)

**职责:** 提供简单的启动/停止控制界面。

**UI 结构:**
- 导航图标 (`ImageView` + `ic_navigation.xml` 矢量图)
- 状态文本 (`TextView` id=`status_text`)
- "启动服务"按钮 (`Button` id=`btn_start`)
- "停止服务"按钮 (`Button` id=`btn_stop`)

**关键方法:**

| 方法 | 说明 |
|------|------|
| `onCreate()` | 初始化视图，绑定按钮点击事件 |

**交互逻辑:**
- 点击"启动服务" → 调用 `NavBleService.start(this)` → 更新状态文本为"服务已启动，等待高德导航广播..."
- 点击"停止服务" → 调用 `NavBleService.stop(this)` → 更新状态文本为"服务已停止"

**Activity 配置:**
- `android:launchMode="singleTop"` — 避免重复创建实例
- 作为 LAUNCHER Activity，是应用唯一入口

---

### 4.3 NavBleService — 前台服务

**文件:** [NavBleService.kt](file:///workspace/app/src/main/java/com/navblerelay/service/NavBleService.kt)

**职责:** 核心协调器，作为 Android 前台服务运行，负责：
1. 启动 BLE GATT 服务器
2. 注册高德导航广播监听
3. 将广播数据桥接到 BLE 发送
4. 维护持久通知栏

**清单声明:**
```xml
<service
    android:name=".service.NavBleService"
    android:exported="false"
    android:foregroundServiceType="connectedDevice" />
```

**Companion 静态方法:**

| 方法 | 说明 |
|------|------|
| `start(context)` | 启动前台服务（Android O+ 使用 `startForegroundService`） |
| `stop(context)` | 发送 `ACTION_STOP` 停止服务 |

**生命周期回调:**

| 回调 | 行为 |
|------|------|
| `onCreate()` | 创建通知渠道 → 启动前台通知 → 初始化 `BleGattServer` → 设置 BLE 回调 → 启动 BLE 服务 → 注册广播接收器 |
| `onStartCommand()` | 处理 `ACTION_STOP` 停止请求；返回 `START_STICKY` 保持服务存活 |
| `onBind()` | 返回 `null`（非绑定服务） |
| `onDestroy()` | 注销广播接收器 → 停止 BLE 服务 |

**广播回调桥接（在 `registerBroadcastReceiver()` 中设置）:**

| 广播回调 | BLE 转发方法 | 附加通知更新 |
|----------|-------------|-------------|
| `onGuideInfo` | `bleServer.sendGuideInfo(info)` | — |
| `onMapState` | `bleServer.sendMapState(state, crossMap)` | 导航中/已结束/到达目的地 |
| `onDriveWay` | `bleServer.sendDriveWay(info)` | — |
| `onTmcSegment` | `bleServer.sendTmcSegment(info)` | — |
| `onLocation` | `bleServer.sendLocation(info)` | — |

**通知管理:**

| 方法 | 说明 |
|------|------|
| `createNotificationChannel()` | Android O+ 创建通知渠道 "导航BLE转发" |
| `createNotification()` | 构建前台通知，含"停止"操作按钮 |
| `updateNotification(text)` | 动态更新通知文本（如连接状态、导航状态） |

**通知栏交互:**
- 点击通知 → 回到 MainActivity
- 点击"停止"按钮 → 发送 `ACTION_STOP` 停止服务

---

### 4.4 NavBroadcastReceiver — 广播接收器

**文件:** [NavBroadcastReceiver.kt](file:///workspace/app/src/main/java/com/navblerelay/receiver/NavBroadcastReceiver.kt)

**职责:** 监听高德地图车机版发出的 `AUTONAVI_STANDARD_BROADCAST_SEND` 广播，解析不同 `KEY_TYPE` 对应的数据，并通过回调函数将数据传递给 `NavBleService`。

**回调接口:**

| 回调 | 签名 | 触发 KEY_TYPE |
|------|------|--------------|
| `onGuideInfo` | `(GuideInfo) -> Unit` | 10001 |
| `onMapState` | `(Int, String?) -> Unit` | 10019 |
| `onDriveWay` | `(DriveWayInfo) -> Unit` | 13012 |
| `onTmcSegment` | `(TmcSegmentInfo) -> Unit` | 13011 |
| `onLocation` | `(LocationInfo) -> Unit` | 10065 |

**解析方法:**

| 方法 | 数据来源 | 说明 |
|------|---------|------|
| `parseGuideInfo(intent)` | Intent Extra 键值对 | 逐字段从 Intent 提取，构建 `GuideInfo` 对象 |
| `parseMapState(intent)` | `EXTRA_STATE` + `EXTRA_CROSS_MAP` | 获取导航状态和路口放大图信息 |
| `parseDriveWay(intent)` | `EXTRA_DRIVE_WAY` (JSON) | 解析 JSON 数组获取车道信息列表 |
| `parseTmcSegment(intent)` | `EXTRA_TMC_SEGMENT` (JSON) | 解析 JSON 数组获取路况光柱图数据 |
| `parseLocation(intent)` | `EXTRA_LOCATION_INFO` (JSON) | 解析 JSON 获取定位信息 |

**广播注册方式:**
- Android 13+ (API 33): 使用 `RECEIVER_NOT_EXPORTED` 标志，仅接收系统内部广播
- Android 13 以下: 标准注册

---

### 4.5 AmapAutoProtocol — 协议定义层

**文件:** [AmapAutoProtocol.kt](file:///workspace/app/src/main/java/com/navblerelay/protocol/AmapAutoProtocol.kt)

**职责:** 定义 AmapAuto 标准广播协议的所有常量、映射表和数据模型。

**协议常量 (`AmapAutoProtocol` object):**

| 常量 | 值 | 说明 |
|------|-----|------|
| `ACTION_SEND` | `"AUTONAVI_STANDARD_BROADCAST_SEND"` | 高德发出的广播 Action |
| `ACTION_RECV` | `"AUTONAVI_STANDARD_BROADCAST_RECV"` | 高德接收的广播 Action |
| `KEY_GUIDE_INFO` | 10001 | 引导信息类型 |
| `KEY_MAP_STATE` | 10019 | 地图状态/心跳类型 |
| `KEY_ROUTE_INFO` | 10056 | 路线信息类型 |
| `KEY_LOCATION` | 10065 | 定位信息类型 |
| `KEY_TMC_SEGMENT` | 13011 | 实时交通光柱图类型 |
| `KEY_DRIVE_WAY` | 13012 | 车道信息类型 |

**导航状态码:**

| 常量 | 值 | 含义 |
|------|-----|------|
| `STATE_START_NAV` | 8 | 开始导航 |
| `STATE_STOP_NAV` | 9 | 停止导航 |
| `STATE_ARRIVE_DEST` | 39 | 到达目的地 |

**映射表:**

| 映射表 | 内容 |
|--------|------|
| `ICON_MAP` | 转向图标 ID → 含义（1-20，如"自车""左转""直行"等） |
| `ROAD_TYPE_MAP` | 道路类型 ID → 含义（0-10，如"高速公路""国道"等） |
| `CAMERA_TYPE_MAP` | 电子眼类型 ID → 含义（0-4，如"测速摄像头""闯红灯拍照"等） |

**数据类:**

| 数据类 | 对应 KEY_TYPE | 主要字段 |
|--------|--------------|---------|
| `GuideInfo` | 10001 | 导航类型、道路名称、转向图标、剩余距离/时间、车速、限速、经纬度、电子眼信息、服务区信息、红绿灯数量等 |
| `DriveWayInfo` | 13012 | `enabled`, `size`, `lanes: List<LaneInfo>` |
| `LaneInfo` | — | 车道编号 `number`、车道图标 `backIcon` |
| `TmcSegmentInfo` | 13011 | `enabled`, `size`, 总距离、剩余距离、已完成距离、`segments: List<TmcSegment>` |
| `TmcSegment` | — | 段编号、路况状态（-1~4）、距离、百分比 |
| `LocationInfo` | 10065 | 方位角 `bearing`、精度 `accuracy`、速度 `speed`、时间戳、定位提供者 |
| `BleDataPacket` | 通用 | 类型 `type`、时间戳 `ts`、数据 `Map<String, Any>`（通用传输包，当前未直接使用） |

---

### 4.6 BleGattServer — BLE 通信层

**文件:** [BleGattServer.kt](file:///workspace/app/src/main/java/com/navblerelay/ble/BleGattServer.kt)

**职责:** 将 Android 设备作为 BLE 外设（Peripheral），创建 GATT 服务并通过 5 个特征值（Characteristic）以 Notify 方式向连接的 ESP32 设备推送导航数据。

**UUID 设计:**

| 名称 | UUID | 用途 |
|------|------|------|
| `SERVICE_UUID` | `0000FFE0-...` | 主 GATT 服务 |
| `CHAR_GUIDE_UUID` | `0000FFE1-...` | 引导信息通道 |
| `CHAR_DRIVE_WAY_UUID` | `0000FFE2-...` | 车道信息通道 |
| `CHAR_TMC_UUID` | `0000FFE3-...` | 路况光柱图通道 |
| `CHAR_STATE_UUID` | `0000FFE4-...` | 地图状态通道 |
| `CHAR_LOCATION_UUID` | `0000FFE5-...` | 定位信息通道 |

**特征值属性:** `PROPERTY_NOTIFY | PROPERTY_READ` + `PERMISSION_READ`

**核心方法:**

| 方法 | 说明 |
|------|------|
| `start()` | 检查蓝牙状态 → 打开 GATT Server → 添加服务 → 开始 BLE 广播 |
| `stop()` | 停止广播 → 关闭 GATT Server |
| `addService()` | 创建主 Service 并添加 5 个特征值 |
| `createCharacteristic(uuid)` | 创建支持 Notify + Read 的特征值 |
| `startAdvertising()` | 低延迟 + 高功率广播，包含 SERVICE_UUID 和设备名称 |
| `sendGuideInfo(info)` | 将 `GuideInfo` 序列化为 JSON，通过 0xFFE1 特征值通知 |
| `sendDriveWay(info)` | 将 `DriveWayInfo` 序列化为 JSON，通过 0xFFE2 特征值通知 |
| `sendTmcSegment(info)` | 将 `TmcSegmentInfo` 序列化为 JSON，通过 0xFFE3 特征值通知 |
| `sendMapState(state, crossMap)` | 将导航状态序列化为 JSON，通过 0xFFE4 特征值通知 |
| `sendLocation(info)` | 将 `LocationInfo` 序列化为 JSON，通过 0xFFE5 特征值通知 |
| `notifyCharacteristic(ch, value)` | 底层 Notify 发送，检查连接状态后写入并通知 |

**BLE 广播配置:**
- 模式: `ADVERTISE_MODE_LOW_LATENCY`（低延迟）
- 发射功率: `ADVERTISE_TX_POWER_HIGH`（高功率）
- 超时: 0（无限广播）
- 可连接: `true`
- 广播数据: 设备名称 + SERVICE_UUID

**回调:**

| 回调 | 说明 |
|------|------|
| `onDeviceConnected` | ESP32 连接成功时触发 |
| `onDeviceDisconnected` | ESP32 断开时触发 |
| `onError` | 蓝牙错误（未开启、广播失败等） |

**GATT Server 回调:**

| 事件 | 说明 |
|------|------|
| `onConnectionStateChange` | 跟踪连接/断开状态，更新 `connectedDevice` |
| `onMtuChanged` | 记录 MTU 协商结果（由 ESP32 侧发起） |

**数据格式:**

所有特征值通知均使用 **JSON 字符串**，结构为:
```json
{
  "type": 10001,
  "ts": 1700000000000,
  "data": { ... }
}
```

---

## 5. 数据流与交互流程

### 完整数据流

```
高德地图车机版
    │
    │ sendBroadcast(ACTION_SEND)
    │ 附带 KEY_TYPE + 数据 Extras
    ▼
NavBroadcastReceiver.onReceive()
    │
    │ 根据 KEY_TYPE 分发
    ├── KEY_GUIDE_INFO  → parseGuideInfo()  → GuideInfo
    ├── KEY_MAP_STATE   → parseMapState()   → state + crossMap
    ├── KEY_DRIVE_WAY   → parseDriveWay()   → DriveWayInfo
    ├── KEY_TMC_SEGMENT → parseTmcSegment() → TmcSegmentInfo
    └── KEY_LOCATION    → parseLocation()   → LocationInfo
    │
    │ 回调函数
    ▼
NavBleService (回调桥接)
    │
    │ bleServer.sendXxx()
    ▼
BleGattServer
    │
    │ 序列化为 JSON
    │ notifyCharacteristicChanged()
    ▼
ESP32 (BLE Central)
    │
    │ 接收 JSON 数据
    │ 解析并驱动显示
    ▼
外部硬件显示
```

### 服务生命周期

```
MainActivity
  │ 点击"启动服务"
  ▼
NavBleService.start(context)
  │
  ├─▶ startForegroundService(intent)
  │
  ▼
NavBleService.onCreate()
  │
  ├─▶ createNotificationChannel()    // 创建通知渠道
  ├─▶ startForeground(notification)   // 前台通知
  ├─▶ BleGattServer(this).start()     // 启动 BLE
  │     ├─ openGattServer()
  │     ├─ addService()               // 注册 5 个特征值
  │     └─ startAdvertising()         // 开始广播
  │
  └─▶ registerBroadcastReceiver()     // 注册广播监听
        │
        ▼
    等待高德广播 + ESP32 连接
        │
        ▼
  MainActivity 点击"停止服务"
        │
  NavBleService.stop(context)
        │
        ▼
  NavBleService.onStartCommand()
        │ action == ACTION_STOP
        ▼
  stopSelf() → onDestroy()
        ├─ unregisterReceiver()
        └─ bleServer.stop()
```

---

## 6. BLE 服务与特征值设计

### GATT 服务结构

```
Service: 0000FFE0-0000-1000-8000-00805F9B34FB
│
├── Characteristic: 0000FFE1 (引导信息 GuideInfo)
│   属性: NOTIFY | READ
│   权限: READ
│
├── Characteristic: 0000FFE2 (车道信息 DriveWay)
│   属性: NOTIFY | READ
│   权限: READ
│
├── Characteristic: 0000FFE3 (路况光柱图 TMC)
│   属性: NOTIFY | READ
│   权限: READ
│
├── Characteristic: 0000FFE4 (地图状态 MapState)
│   属性: NOTIFY | READ
│   权限: READ
│
└── Characteristic: 0000FFE5 (定位信息 Location)
   属性: NOTIFY | READ
   权限: READ
```

### 数据分类

| 特征值 | 数据内容 | 传输频率（典型） |
|--------|---------|----------------|
| 0xFFE1 (引导信息) | 当前道路、转向图标、剩余距离、车速、限速、电子眼、服务区 | 每秒 1 次 |
| 0xFFE2 (车道信息) | 车道数量、各车道导向箭头 | 接近路口时触发 |
| 0xFFE3 (路况光柱图) | 整条路线的拥堵状态分段 | 路线变化时触发 |
| 0xFFE4 (地图状态) | 导航开始/停止/到达、路口放大图 | 状态变化时触发 |
| 0xFFE5 (定位信息) | GPS 方位角、精度、速度 | 每秒 1 次 |

### ESP32 端交互约定

- ESP32 扫描并连接 Android 设备的 BLE 广播
- 连接后 ESP32 侧发起 MTU 协商（通常设为 512）
- 订阅所有 5 个特征值的 Notify
- 收到 JSON 数据后按 `type` 字段区分数据类型并解析

---

## 7. 权限与安全

### 运行时权限

| 权限 | 用途 | 保护级别 |
|------|------|---------|
| `BLUETOOTH` | 基础蓝牙操作 | Normal |
| `BLUETOOTH_ADMIN` | 蓝牙管理（发现设备） | Normal |
| `BLUETOOTH_ADVERTISE` | BLE 广播 | Normal |
| `BLUETOOTH_CONNECT` | 蓝牙连接 (API 31+) | Dangerous |
| `ACCESS_FINE_LOCATION` | 精确位置（Android 12+ 蓝牙扫描需要） | Dangerous |
| `FOREGROUND_SERVICE` | 前台服务运行 | Normal |
| `FOREGROUND_SERVICE_CONNECTED_DEVICE` | 前台服务类型：已连接设备 (API 34+) | Normal |
| `POST_NOTIFICATIONS` | 发送通知 (API 33+) | Dangerous |

**注意:** 当前代码中未请求运行时权限（`BLUETOOTH_CONNECT`、`ACCESS_FINE_LOCATION`、`POST_NOTIFICATIONS` 需要动态申请）。如需在实际设备上运行，需在 `MainActivity` 中添加权限请求逻辑。

### 硬件要求

| 特性 | 是否必需 |
|------|---------|
| `android.hardware.bluetooth_le` | **必需** (required=true) |
| `android.hardware.location` | 可选 (required=false) |

### ProGuard 规则

```
-keep class com.navblerelay.protocol.** { *; }
-dontwarn com.navblerelay.**
```

保留 `protocol` 包下所有类（数据类用于序列化/反序列化），抑制整个包的警告。

---

## 8. 项目运行方式

### 前置条件

1. **Android Studio** (推荐 Hedgehog 2024.1+ 或更新版本)
2. **JDK 17**
3. **Android SDK** (API 35)
4. 一台支持 BLE 外设模式的 Android 设备（Android 5.0+）
5. 安装高德地图车机版（AmapAuto）

### 本地运行步骤

```bash
# 1. 克隆项目
git clone <repository-url>
cd nav-ble-relay

# 2. 使用 Android Studio 打开项目
# File → Open → 选择 nav-ble-relay 目录

# 3. 等待 Gradle Sync 完成

# 4. 连接 Android 设备（开启 USB 调试）

# 5. 运行
# 点击 Run 按钮或执行:
./gradlew installDebug
```

### 命令行构建

```bash
# Debug 构建
./gradlew assembleDebug

# Release 构建（已启用 ProGuard 混淆）
./gradlew assembleRelease

# 输出路径: app/build/outputs/apk/
```

### 使用流程

1. 安装 APK 到 Android 设备
2. 打开"导航BLE转发"应用
3. 点击"启动服务"按钮
4. 打开高德地图车机版并开始导航
5. ESP32 设备扫描并连接 Android 设备
6. 导航数据通过 BLE 实时转发到 ESP32

---

## 9. 构建与发布

### Gradle 配置要点

- **JVM 堆内存:** 2048MB (`org.gradle.jvmargs=-Xmx2048m`)
- **AndroidX:** 已启用
- **非传递 R 类:** 已启用 (`android.nonTransitiveRClass=true`)
- **Release 混淆:** 已启用 (`isMinifyEnabled = true`)
- **Java 兼容性:** 源代码和目标均使用 Java 17

### 发布检查清单

- [ ] 确保 `minSdk` 与实际目标设备兼容
- [ ] 添加运行时权限请求代码
- [ ] 配置 Release 签名
- [ ] 验证 ProGuard 规则正确性
- [ ] 测试在高德地图不发出广播时的降级行为

---

## 10. 目录结构总览

```
nav-ble-relay/
├── app/
│   ├── build.gradle.kts                  # 应用模块构建配置
│   ├── proguard-rules.pro                # ProGuard 混淆规则
│   └── src/
│       └── main/
│           ├── AndroidManifest.xml        # 应用清单（权限、组件声明）
│           ├── java/com/navblerelay/
│           │   ├── NavBleApp.kt           # Application 入口
│           │   ├── MainActivity.kt        # 主界面 Activity
│           │   ├── ble/
│           │   │   └── BleGattServer.kt   # BLE GATT 服务（外设模式）
│           │   ├── protocol/
│           │   │   └── AmapAutoProtocol.kt # 协议定义 + 数据模型
│           │   ├── receiver/
│           │   │   └── NavBroadcastReceiver.kt # 高德广播接收器
│           │   └── service/
│           │       └── NavBleService.kt   # 前台服务（核心协调器）
│           └── res/
│               ├── drawable/
│               │   └── ic_navigation.xml  # 导航图标（矢量图）
│               ├── layout/
│               │   └── activity_main.xml  # 主界面布局
│               ├── mipmap-anydpi-v26/
│               │   └── ic_launcher.xml    # 自适应启动图标
│               └── values/
│                   ├── colors.xml         # 颜色定义
│                   ├── strings.xml        # 字符串资源
│                   └── themes.xml         # Material 主题定义
├── build.gradle.kts                      # 项目级构建配置
├── settings.gradle.kts                   # 项目设置（模块声明）
├── gradle.properties                     # Gradle 属性
├── gradlew / gradlew.bat                 # Gradle Wrapper 脚本
└── gradle/wrapper/                       # Gradle Wrapper 文件
```

---

## 附录：关键类关系图

```
NavBleApp (Application)
    │
MainActivity (Activity)
    │  调用 NavBleService.start() / stop()
    ▼
NavBleService (Service) ─────────────────────────────┐
    │  持有                                             │
    ├── BleGattServer (ble/)                            │
    │   ├── 5 个 BluetoothGattCharacteristic            │
    │   ├── BluetoothLeAdvertiser                       │
    │   └── BluetoothGattServer                         │
    │                                                   │
    └── NavBroadcastReceiver (receiver/)                │
        ├── parseGuideInfo()  → GuideInfo               │
        ├── parseMapState()   → (state, crossMap)       │
        ├── parseDriveWay()   → DriveWayInfo → LaneInfo │
        ├── parseTmcSegment() → TmcSegmentInfo → TmcSegment
        └── parseLocation()   → LocationInfo            │
                                                        │
AmapAutoProtocol (protocol/, object) ◄─────────────────┘
    │  常量定义
    ├── ACTION_SEND / ACTION_RECV
    ├── KEY_TYPE 系列
    ├── STATE 系列
    └── 映射表 (ICON_MAP, ROAD_TYPE_MAP, CAMERA_TYPE_MAP)
```

---

> **文档版本:** 1.0 | **对应代码版本:** 1.0.0