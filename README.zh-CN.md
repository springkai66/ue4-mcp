# UnrealMCP — UE4.26 修改版

中文 | [English](README.md)

基于 [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp)（上游提交 `4e5f00da5073`）修改，属于独立派生版本。上游主要面向 UE5，本仓库面向 UE4.26，由 [springkai66](https://github.com/springkai66) 维护，包含 **C++ 编辑器插件与 Python MCP 服务端**。

```text
AI 客户端 → MCP stdio → Python 服务 → TCP 127.0.0.1:55557 → Unreal 编辑器插件
```

## 核心功能与主要命令

本仓库为 **C++ 编辑器插件 + MCP 服务端**，让 AI 客户端驱动 UE4.26：关卡与 Actor 操作、蓝图资产与图表编排、UMG 控件组合、材质、工程设置以及资产属性写入。反射属性写入支持 Vector、Rotator、FIntPoint、数组和 TSet；资产保存带直存回退，失败时返回具体原因而不是静默成功。

- **编辑器与 Actor** — `get_actors_in_level`、`find_actors_by_name`、`spawn_actor`、`spawn_blueprint_actor`、`delete_actor`、`set_actor_transform`、`get_actor_properties`、`set_actor_property`、`focus_viewport`、`duplicate_level`
- **蓝图资产** — `create_blueprint`、`add_component_to_blueprint`、`set_component_property`、`set_static_mesh_properties`、`set_physics_properties`、`set_blueprint_property`、`set_pawn_properties`、`compile_blueprint`
- **蓝图图表** — `add_blueprint_event_node`、`add_blueprint_input_action_node`、`add_blueprint_function_node`、`add_blueprint_variable`、`add_blueprint_get_self_component_reference`、`add_blueprint_self_reference`、`connect_blueprint_nodes`、`find_blueprint_nodes`、`get_blueprint_node_details`、`delete_blueprint_node`
- **图表排版** — `get_blueprint_graph`、`format_blueprint_graph`
- **UMG 控件** — `create_umg_widget_blueprint`、`add_text_block_to_widget`、`add_button_to_widget`、`add_image_to_widget`、`list_widget_blueprints`、`get_widget_tree`、`add_widget_to_widget`、`bind_widget_event`、`set_text_block_binding`、`add_widget_to_viewport`
- **材质** — `create_material`、`add_vector_parameter`、`set_material_on_component`
- **工程与资产** — `create_input_mapping`；插件端还实现了 DataTable 行读写（`create_data_table`、`add_row`、`read_row`）与任意资产属性写入（`set_asset_property`），供直接使用 TCP 协议的客户端调用。
- **调试** — `get_output_log`

## 快速开始

前置要求：**Windows + UE4.26.2**、对应版本的 C++ 编译工具链，以及 [uv](https://docs.astral.sh/uv/getting-started/installation/)。无需手动安装 Python 依赖：客户端首次启动服务时，uv 会按 `uv.lock` 自动创建环境。

### 第 1 步：安装插件并编译

1. 将整个仓库复制到 `<你的工程>/Plugins/UnrealMCP`，确保 `UnrealMCP.uplugin` 直接位于该目录。纯蓝图工程先添加一个 C++ 类，使工程可以编译插件。
2. 生成工程文件，编译工程的 Editor 目标。
3. 在 **编辑 → 插件** 中启用 **Editor Scripting Utilities** 和 **UnrealMCP**，重启编辑器。
4. 输出日志出现 `UnrealMCPBridge: Server started on 127.0.0.1:55557` 即成功。调用工具时保持编辑器开启；同一时刻只能有一个编辑器占用该端口。

### 第 2 步：接入 MCP 客户端

无需手动启动服务：客户端通过 stdio 自动拉起 Python 服务（`55557` 是插件 TCP 端口，不能填成 HTTP/SSE 地址；本服务不需要 API Key）。

把下面的服务条目合并进客户端配置，`C:/path/to/unreal-mcp/Python` 换成 `Python/` 目录的绝对路径：

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

写入位置：

| 客户端 | 配置位置 |
| --- | --- |
| **Claude Code** | 一条命令即可：`claude mcp add --scope user unreal-mcp -- uv --directory "C:/path/to/unreal-mcp/Python" run --frozen unreal_mcp_server.py`，用 `/mcp` 查看状态 |
| **Pi** | `~/.pi/agent/mcp.json`（用户级）或项目 `.pi/mcp.json` |
| **Codex** | `~/.codex/config.toml`，TOML 格式见下 |
| **OpenCode** | `opencode.json`，结构见下 |
| **WorkBuddy**（腾讯桌面版） | 侧边栏 **插件 → MCP 服务器 → 配置 MCP**，即 `~/.workbuddy/mcp.json` 或项目 `.workbuddy/mcp.json` |
| **ZCode**（Z.ai 桌面版） | **Settings → MCP Servers → New MCP Server → Full configuration**，粘贴上面的 JSON |

只有两个客户端格式不同：

Codex（`~/.codex/config.toml`）：

```toml
[mcp_servers.unreal-mcp]
command = "uv"
args = ["--directory", "C:/path/to/unreal-mcp/Python", "run", "--frozen", "unreal_mcp_server.py"]
```

OpenCode（`opencode.json`）：

```json
{
  "mcp": {
    "unreal-mcp": {
      "type": "local",
      "command": ["uv", "--directory", "C:/path/to/unreal-mcp/Python", "run", "--frozen", "unreal_mcp_server.py"],
      "enabled": true
    }
  }
}
```

`~` 表示用户主目录（Windows 通常是 `%USERPROFILE%`）。

### 第 3 步：验证

对客户端说：**“使用 unreal-mcp 列出当前关卡中的 Actor。”** 编辑器未开启时也能发现工具，实际操作需要插件已监听。日常顺序：先开编辑器，再从客户端调用。

## 蓝图整洁规范

排版由插件实现：未指定坐标的节点自动落位，`compile_blueprint` 会在编译前整理本次会话生成的节点；`get_blueprint_graph` 可读取节点几何，`format_blueprint_graph`（显式 `node_ids` 或 `whole_graph=true`）用于整理已有图表。排版只移动节点，保留连接与行为；循环和共享节点保留并给出警告，不承诺零交叉。

UMG 界面建议先提供示意图或确认文字布局。插件提供 `list_widget_blueprints`、`get_widget_tree` 和 `add_widget_to_widget`，支持原生与项目控件，并校验 Canvas/Box/Overlay/Border/SizeBox 的槽位布局。AI 指导仍要求对齐与分区，机械部分已由插件执行。

## 开发与测试

运行 Python 测试：

```sh
uv --directory "C:/path/to/unreal-mcp/Python" run --frozen python -m unittest discover -s tests
```

Unreal 控制台：`Automation RunTests UnrealMCP`。`Python/scripts/` 中的手动脚本会创建或修改编辑器内容，应按需执行。Python 接口迁移说明见 [Python/README.md](Python/README.md)。

## 许可

MIT；上游署名及许可声明依据见 [LICENSE](LICENSE) 和 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
