# 修复 Android 15 闪退问题 Spec

## Why
在 Android 15 设备上启动服务时闪退。原因是 Android 12+ 的 `BLUETOOTH_CONNECT` 和 Android 13+ 的 `POST_NOTIFICATIONS` 属于危险权限，需在运行时动态申请，否则 `startForeground()` 和 `openGattServer()` 会抛出 `SecurityException` 导致崩溃。同时广播接收器使用 `RECEIVER_NOT_EXPORTED` 导致无法接收跨应用的高德导航广播。

## What Changes
- 在 `AndroidManifest.xml` 中添加 `neverForLocation` 标记，避免 Android 12+ 强制要求位置权限
- 在 `MainActivity` 中添加运行时权限请求逻辑（`BLUETOOTH_CONNECT`、`POST_NOTIFICATIONS`）
- 修复 `NavBleService` 中广播接收器注册为 `RECEIVER_EXPORTED`（跨应用广播需要）
- 在 `NavBleService.onCreate()` 中添加权限检查，避免未授权时崩溃

## Impact
- Affected specs: CI/CD 构建流程
- Affected code: `AndroidManifest.xml`, `MainActivity.kt`, `NavBleService.kt`

## ADDED Requirements
### Requirement: 运行时权限请求
应用 SHALL 在启动服务前请求必要的运行时权限，避免因权限不足导致崩溃。

#### Scenario: 首次启动缺少权限
- **WHEN** 用户点击"启动服务"且未授予通知或蓝牙权限
- **THEN** 系统弹出权限请求对话框，用户授权后启动服务

#### Scenario: 权限已授予
- **WHEN** 用户点击"启动服务"且所有权限已授予
- **THEN** 直接启动服务

#### Scenario: 权限被拒绝
- **WHEN** 用户拒绝权限请求
- **THEN** 显示提示信息，服务不启动

### Requirement: 跨应用广播接收
广播接收器 SHALL 注册为 `RECEIVER_EXPORTED` 以接收来自高德地图的跨应用广播。

#### Scenario: 高德地图发送导航广播
- **WHEN** 高德地图发送 `AUTONAVI_STANDARD_BROADCAST_SEND` 广播
- **THEN** 应用正确接收并解析广播数据