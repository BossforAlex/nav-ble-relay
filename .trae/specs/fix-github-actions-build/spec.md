# 修复 GitHub Actions CI 编译报错 Spec

## Why
当前 GitHub Actions CI 工作流 (`build.yml`) 中使用了不存在的 Action 版本 (`@v5`)，且 Android SDK 安装步骤存在兼容性问题，导致 CI 编译失败。

## What Changes
- 修正所有 Action 版本号从 `@v5` 降级到实际存在的 `@v4`
- 修复 Android SDK 安装步骤，使用更可靠的 `android-actions/setup-android` Action 替代手动 sdkmanager 命令
- 修复 `local.properties` 文件名拼写错误（应为 `local.properties`）

## Impact
- Affected specs: CI/CD 构建流程
- Affected code: `.github/workflows/build.yml`

## MODIFIED Requirements
### Requirement: GitHub Actions CI 构建
CI 工作流 SHALL 使用正确版本的 GitHub Actions 和可靠的 Android SDK 安装方式，确保 `assembleDebug` 和 `assembleRelease` 编译成功。

#### Scenario: Push 到 main 分支触发构建
- **WHEN** 代码推送到 main 或 master 分支
- **THEN** CI 自动执行 Debug 和 Release APK 构建，并上传产物

#### Scenario: PR 触发构建
- **WHEN** 创建针对 main 或 master 的 Pull Request
- **THEN** CI 自动执行构建验证，Gradle 缓存为只读模式

#### Scenario: 手动触发构建
- **WHEN** 通过 workflow_dispatch 手动触发
- **THEN** CI 执行完整构建流程