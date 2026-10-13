# UnrealMCP — UE4.26 修改版

中文 | [English](README.md)

基于 [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp)（上游提交 `4e5f00da5073`）修改，属于独立派生版本。上游主要面向 UE5，本仓库面向 UE4.26，由 [springkai66](https://github.com/springkai66) 维护，包含 **C++ 编辑器插件与 Python MCP 服务端**。

```text
AI 客户端 → MCP stdio → Python 服务 → TCP 127.0.0.1:55557 → Unreal 编辑器插件
```

## 主要改动

- 适配 UE4.26；修正 Blueprint/UMG 创建、反射属性写入和 DataTable 行处理。
- 修复 JSON 字节分帧、UTF-8 分包、部分发送、socket 释放及停止流程。
- 增加资产路径校验、保存/编译失败反馈，以及蓝图图表 Schema 连线检查。
- 完善 UMG 文字、按钮和事件编辑，增加材质及输出日志工具；Python 暴露 35 个工具。
- 修复 Python 响应解包、工具注册和连接处理，合并脚本传输代码并补充回归检查；发布副本移除项目专有路径和名称。

## 启用插件

准备：**Windows + UE4.26.2**、对应版本的 C++ 编译工具链、**Python 3.10+** 和 [uv](https://docs.astral.sh/uv/getting-started/installation/)。

1. 将整个仓库复制到 `<你的工程>/Plugins/UnrealMCP`，确保 `UnrealMCP.uplugin` 直接位于该目录。纯蓝图工程先添加一个 C++ 类，使工程可以编译插件。
2. 生成工程文件，用对应 UE4 工具链编译工程的 Editor 目标。
3. 在 **编辑 → 插件** 中启用 **Editor Scripting Utilities** 和 **UnrealMCP**，重启编辑器。
4. 在输出日志中确认 `UnrealMCPBridge: Server started on 127.0.0.1:55557`。调用工具时保持编辑器开启；同一时刻只能有一个编辑器使用该端口。
5. 安装一次 Python 依赖：

   ```sh
   uv --directory "C:/path/to/unreal-mcp/Python" sync --frozen
   ```

下文路径均替换为仓库或已安装插件中 **`Python/` 的绝对路径**。桌面客户端找不到 `uv` 时，将命令改为 `uv` 可执行文件的绝对路径。已有配置应合并服务条目。

## 接入 MCP 客户端

客户端通过 **stdio** 启动 Python 服务；`55557` 是插件的原始 TCP 端口，不能作为 HTTP/SSE MCP 地址。无需单独启动服务，本桥接本身不需要 API Key。

### Claude Code

```sh
claude mcp add --transport stdio --scope user unreal-mcp -- uv --directory "C:/path/to/unreal-mcp/Python" run --frozen unreal_mcp_server.py
```

用 `/mcp` 查看状态。项目级配置可改为 `--scope project`，或把下方 JSON 写入项目 `.mcp.json`。

### Pi、WorkBuddy、ZCode

通用 JSON：

```json
{
  "mcpServers": {
    "unreal-mcp": {
      "command": "uv",
      "args": ["--directory", "C:/path/to/unreal-mcp/Python", "run", "--frozen", "unreal_mcp_server.py"]
    }
  }
}
```

| 客户端 | 配置入口 |
| --- | --- |
| **Pi** | 用户 `~/.pi/agent/mcp.json` 或项目 `.pi/mcp.json`；用 `/mcp` 查看状态。桌面发行版使用其实际生效的 MCP 配置目录。 |
| **WorkBuddy**（腾讯桌面版） | 侧边栏 **插件 → MCP 服务器 → 配置 MCP**；保存到用户 `~/.workbuddy/mcp.json` 或项目 `.workbuddy/mcp.json`。 |
| **ZCode**（Z.ai 桌面版） | **Settings → MCP Servers → New MCP Server → Full configuration**；选择 User/Workspace，粘贴 JSON，添加并启用。这是界面输入格式，原生 `.zcode` 文件使用 `mcp.servers`。 |

`~` 表示用户主目录，Windows 通常对应 `%USERPROFILE%`。

### Codex

写入 `~/.codex/config.toml`，或受信任项目的 `.codex/config.toml`：

```toml
[mcp_servers.unreal-mcp]
command = "uv"
args = ["--directory", "C:/path/to/unreal-mcp/Python", "run", "--frozen", "unreal_mcp_server.py"]
```

也可执行：`codex mcp add unreal-mcp -- uv --directory "C:/path/to/unreal-mcp/Python" run --frozen unreal_mcp_server.py`。用 `codex mcp list` 检查。

### OpenCode

合并到项目 `opencode.json` 或 `~/.config/opencode/opencode.json`：

```json
{
  "$schema": "https://opencode.ai/config.json",
  "mcp": {
    "unreal-mcp": {
      "type": "local",
      "command": ["uv", "--directory", "C:/path/to/unreal-mcp/Python", "run", "--frozen", "unreal_mcp_server.py"],
      "enabled": true
    }
  }
}
```

用 `opencode mcp list` 检查。连接任意客户端后，可以说：**“使用 unreal-mcp 列出当前关卡中的 Actor。”** 编辑器未开启时也能发现工具，但实际编辑器操作需要插件已监听。

配置参考：[Claude Code](https://code.claude.com/docs/en/mcp)、[Codex](https://developers.openai.com/codex/mcp/)、[Pi](https://github.com/earendil-works/pi/blob/main/packages/coding-agent/docs/mcp.md)、[OpenCode](https://opencode.ai/docs/mcp-servers/)、[WorkBuddy](https://www.codebuddy.ai/docs/zh/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/MCP-Guide)、[ZCode](https://zcode.z.ai/en/docs/mcp-services)。

## 蓝图整洁规范

排版由插件实现：未指定坐标的节点自动落位，`compile_blueprint` 会在编译前整理本次会话生成的节点；`get_blueprint_graph` 可读取节点几何，`format_blueprint_graph`（显式 `node_ids` 或 `whole_graph=true`）用于整理已有图表。排版只移动节点，保留连接与行为；循环和共享节点保留并给出警告，不承诺零交叉。

UMG 界面建议先提供示意图或确认文字布局。插件提供 `list_widget_blueprints`、`get_widget_tree` 和 `add_widget_to_widget`，支持原生与项目控件，并校验 Canvas/Box/Overlay/Border/SizeBox 的槽位布局。AI 指导仍要求对齐与分区，机械部分已由插件执行。

## 验证与限制

UE4.26.2 Win64 插件编译及隔离宿主中的 12 个自动化用例通过（含图表排版与 UMG 组合套件）；Python 的 31 个本地测试通过，含 stdio 到假 TCP 编辑器的工具调用。六种客户端配置依据官方文档，未全部逐一实机验证。UE5、Mac/Linux、运行时 UMG 显示和编辑器重启后的持久化尚未完整验证。

已知风险：发布前原始宿主工程的自动化运行曾以堆损坏（0xC0000374）退出。隔离宿主用例全部通过，无法归因该退出的原因；本版本新增的代码路径（离屏 Slate 节点测量、编辑器对象标志处理）**未被排除嫌疑**，在该工程完成一次干净运行之前，应视为插件未经该环境验证。

清除源 WidgetTree 的 `RF_ArchetypeObject` 与引擎 `UWidgetBlueprint::PostLoad` 行为一致：编辑器界面创建的资产通常已无此标志，此时为空操作；但执行这些命令后保存，会把清除后的标志持久化进资产。

```sh
uv --directory "C:/path/to/unreal-mcp/Python" run --frozen python -m unittest discover -s tests
```

Unreal 控制台：`Automation RunTests UnrealMCP`。`Python/scripts/` 中的手动脚本会创建或修改编辑器内容，应按需执行。Python 接口迁移说明见 [Python/README.md](Python/README.md)。

## 许可

MIT；上游署名及许可声明依据见 [LICENSE](LICENSE) 和 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
