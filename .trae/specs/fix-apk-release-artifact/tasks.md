# Tasks

- [x] Task 1: 修改 GitHub Actions 工作流，添加 GitHub Release 上传步骤
  - [x] SubTask 1.1: 添加 `permissions: contents: write` 到 workflow
  - [x] SubTask 1.2: 添加 `softprops/action-gh-release@v2` 步骤，仅在 push 到 main/master 或 workflow_dispatch 时执行
  - [x] SubTask 1.3: 将 Debug APK 作为 Release 资产上传（文件名 `nav-ble-relay-debug.apk`）

# Task Dependencies
- 所有子任务无依赖关系