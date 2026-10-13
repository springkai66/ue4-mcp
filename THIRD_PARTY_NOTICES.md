# 第三方来源与许可

## 上游项目

本插件是下列开源项目的派生版本（editor 插件部分）：

- 项目：**chongdashu/unreal-mcp**
- 地址：https://github.com/chongdashu/unreal-mcp
- 上游分支／提交：`main` @ `4e5f00da5073`（2025-04-22）

上游仓库在本分支创建时**未包含 `LICENSE` 文件**，也**未在源码中保留版权头**；其许可意图仅以上游 `README.md` 声明为准，该文件写有：

> [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
>
> ## License
> MIT

本仓库据此按 MIT 处理上游代码。由于上游未声明具体的版权年份与版权主体，**本文档不代为推测或补写**；如需在正式分发中补全，请以上游仓库或其作者的书面说明为准。

## 本分支的修改范围

本分支在上游基础上做了以下改动（详见仓库提交历史）：

- **UE4.26 适配**：修正 Widget 蓝图创建（`UWidgetBlueprint`）、编译与保存流程，使资产在编辑器重启后可加载。
- **资产路径参数化**：蓝图与 UMG 创建不再限定固定目录；短名查找先查通用目录，未命中时通过资产注册表在 `/Game` 下按名全局解析（同名多份时拒绝并要求传入完整路径）。
- **接收层重写**：以字节级 JSON 分帧替代定长缓冲，修正越界写入、分包／粘包、UTF-8 字节长度与部分发送；发送路径带截止时间。
- **生命周期**：服务器停止改为先置会话取消标志再等待工作线程退出；socket 由所属 SocketSubsystem 释放。
- **UMG 能力**：按钮与事件绑定、TextBlock 动态绑定、`add_widget_to_viewport` 在 PIE 中实际创建实例。
- **新增自动化测试**：`Source/UnrealMCP/Private/Tests/UnrealMCPRegressionTests.cpp`。
- **清理项目专有信息**：移除内部目录常量与项目名硬编码，测试名不再带项目前缀。

## Python 客户端

本仓库**不包含** Python 客户端。上游的 Python 端参考实现位于上游仓库的 `Python/` 目录；使用本项目时请以其为准，并自行确认与其版本兼容。
