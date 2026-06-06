# 修复 APK Artifact 下载与安装问题 Spec

## Why
GitHub Actions `upload-artifact` 始终将产物打包为 zip 文件，用户下载后需额外解压。且 Release APK 未签名无法安装（错误码 33: packageInfo is null）。用户需要可直接下载安装的 APK 文件。

## What Changes
- 新增 GitHub Release 步骤，上传 APK 作为直接可下载的独立文件（非 zip）
- 仅上传 Debug APK 到 Release（已自动使用 debug keystore 签名，可直接安装）
- Release APK 仍通过 artifact 留存但不发布到 Release（未签名无法安装）
- 添加 `permissions: contents: write` 以支持创建 Release

## Impact
- Affected specs: CI/CD 构建流程
- Affected code: `.github/workflows/build.yml`

## ADDED Requirements
### Requirement: APK 直接下载
CI 构建成功后 SHALL 将已签名的 Debug APK 作为 GitHub Release 资产上传，用户可直接下载 APK 文件而无需解压 zip。

#### Scenario: Push 到 main 分支
- **WHEN** 代码推送到 main 或 master 分支
- **THEN** CI 自动创建/更新 GitHub Release，上传 `app-debug.apk` 作为直接下载资产

#### Scenario: 手动触发构建
- **WHEN** 通过 workflow_dispatch 手动触发
- **THEN** CI 完成构建后同样创建 Release 并上传 APK

#### Scenario: PR 构建
- **WHEN** 创建 Pull Request
- **THEN** 仅通过 artifact 上传产物，不创建 Release