# Tasks

- [ ] Task 1: 修改 AndroidManifest.xml 添加 neverForLocation 标记
  - [ ] SubTask 1.1: 为 `BLUETOOTH_ADVERTISE`、`BLUETOOTH_CONNECT`、`ACCESS_FINE_LOCATION` 添加 `android:usesPermissionFlags="neverForLocation"` 和 `android:maxSdkVersion="30"`

- [ ] Task 2: 修改 MainActivity 添加运行时权限请求
  - [ ] SubTask 2.1: 添加权限请求逻辑，在点击启动服务前检查 `BLUETOOTH_CONNECT` 和 `POST_NOTIFICATIONS`
  - [ ] SubTask 2.2: 处理权限授予/拒绝回调，权限授予后启动服务

- [ ] Task 3: 修复 NavBleService 广播接收器注册
  - [ ] SubTask 3.1: 将 `RECEIVER_NOT_EXPORTED` 改为 `RECEIVER_EXPORTED`（跨应用广播需要）

# Task Dependencies
- Task 2 和 Task 3 可并行执行