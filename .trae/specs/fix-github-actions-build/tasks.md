# Tasks

- [x] Task 1: 修复 GitHub Actions 工作流中的 Action 版本和 SDK 配置
  - [x] SubTask 1.1: 将 `actions/checkout@v5` 改为 `actions/checkout@v4`
  - [x] SubTask 1.2: 将 `actions/setup-java@v5` 改为 `actions/setup-java@v4`
  - [x] SubTask 1.3: 将 `actions/upload-artifact@v5` 改为 `actions/upload-artifact@v4`
  - [x] SubTask 1.4: 将 `gradle/actions/setup-gradle@v5` 改为 `gradle/actions/setup-gradle@v4`
  - [x] SubTask 1.5: 将手动 sdkmanager 安装 Android SDK 的步骤替换为 `android-actions/setup-android@v3` Action，确保 `platforms;android-35` 和 `build-tools;35.0.0` 正确安装

# Task Dependencies
- 所有子任务无依赖关系，可并行修改（同一文件）