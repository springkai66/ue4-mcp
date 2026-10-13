# UnrealMCP (UE4.26 fork)

Unreal Engine 编辑器插件：通过 TCP 上的 JSON 协议让外部工具（MCP 客户端、脚本）驱动编辑器操作，
包括 Actor 管理、蓝图与蓝图节点、UMG 控件、DataTable、材质等。

> **这是派生版本，不是上游的官方仓库。**
> 上游项目：[**chongdashu/unreal-mcp**](https://github.com/chongdashu/unreal-mcp)（MIT）。
> 本仓库只包含**编辑器插件（C++）**部分，不含 Python 客户端与其示例工程。
> 上游面向 UE5，本分支的主要目标是让它**在 UE4.26 上可用**。请勿把本仓库的问题反馈给上游作者。

## 与上游的差异

主要改动集中在“让 UE4.26 上跑通”和“修掉会崩或静默出错的路径”：

- **UE4.26 适配**：Widget 蓝图必须用 `UWidgetBlueprint` 创建，并在编译后保存，否则重启后资产不可加载。
- **资产路径参数化**：创建蓝图／UMG 可指定 `path`；短名查找先查通用目录，未命中则通过资产注册表在
  `/Game` 下按名全局解析，**同名多份时拒绝**并要求调用方传完整路径。
- **接收层重写**：改为字节级 JSON 分帧，修掉定长缓冲区越界写入、分包与粘包、UTF-8 字节长度误用、
  部分发送；发送路径带截止时间。
- **生命周期**：停止服务器时先置会话取消标志，再等待工作线程退出，避免“停止时死锁”。
- **UMG 能力**：按钮与按钮文字、事件绑定、TextBlock 动态绑定、`add_widget_to_viewport` 在 PIE 中真实创建实例。
- **更多命令**：蓝图 Cast 节点、外部成员读取节点、结构体属性、`new_level` / `open_level` / `duplicate_level`、
  材质参数、输出日志等。
- **自动化测试**：`Source/UnrealMCP/Private/Tests/UnrealMCPRegressionTests.cpp`（10 个用例）。

完整改动与验证记录见提交历史；上游署名与许可见 [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md)。

## 环境与验证状态

| 项目 | 说明 |
| --- | --- |
| 开发与验证引擎 | **UE 4.26.2**，Win64，Development |
| 目标平台 | Windows（插件声明还包含 Mac / Linux，但**未经验证**） |
| 依赖 | 引擎插件 `EditorScriptingUtilities`（已在 `.uplugin` 中启用） |

**验证范围**：在本机 UE4.26.2 上，插件模块在真实项目目标下编译、链接通过（本机工程目标、
`-Module=UnrealMCP`）；10 个自动化用例在隔离宿主下全部通过。
**尚未验证**：Mac / Linux、UE5、PIE 中的控件显示效果、长时间运行与真实网络中断场景。
请把本仓库视为“面向 UE4.26 的可用版本”，而不是对所有引擎版本都已验证。

## 安装

1. 把本仓库整个目录放到工程的 `Plugins/UnrealMCP`（或引擎的 `Engine/Plugins/Editor/UnrealMCP`）。
2. 重新生成工程文件并编译 Editor 目标（本插件只声明 `Editor` 模块）。
3. 启动编辑器。日志出现 `UnrealMCPBridge: Server started on 127.0.0.1:55557` 即表示已监听。

不需要把 `Binaries/`、`Intermediate/` 提交或拷贝到其他机器——它们已被 `.gitignore` 忽略。

## 协议

- 默认监听 `127.0.0.1:55557`（仅本机）。
- 请求为 JSON 对象，命令字段用 `type`（兼容旧客户端的 `command`；若两者同为字符串则 `type` 优先）。
  `params` 可省略，提供时必须是对象。
- 响应为 JSON 对象，`status` 为 `success` / `error`，成功时结果在 `result`，失败时信息在 `error`。
- 一次连接可发多条消息，也可分片发送；插件按字节边界切分完整 JSON 对象，**不要求**请求带换行。
- 单条消息上限 1 MiB，嵌套深度上限 128；空闲连接 30 秒后关闭。

请求示例：

```json
{"type": "ping", "params": {}}
```

```json
{"type": "create_umg_widget_blueprint",
 "params": {"name": "WBP_Example", "path": "/Game/UI", "parent_class": "UserWidget"}}
```

## 命令一览（节选）

| 类别 | 命令 |
| --- | --- |
| 基础 | `ping` |
| Actor | `get_actors_in_level`、`find_actors_by_name`、`spawn_actor`、`delete_actor`、`set_actor_transform`、`get_actor_properties`、`set_actor_property`、`set_actor_skeletal_mesh`、`set_world_game_mode`、`spawn_blueprint_actor` |
| 关卡 | `save_level`、`new_level`、`open_level`、`duplicate_level`、`focus_viewport`、`take_screenshot` |
| 蓝图 | `create_blueprint`、`add_component_to_blueprint`、`set_component_property`、`set_physics_properties`、`compile_blueprint`、`set_blueprint_property`、`set_static_mesh_properties`、`set_pawn_properties` |
| 蓝图节点 | `add_blueprint_event_node`、`add_blueprint_function_node`、`add_blueprint_variable`、`add_blueprint_cast_node`、`add_blueprint_external_member_get_node`、`connect_blueprint_nodes`、`find_blueprint_nodes` 等 |
| UMG | `create_umg_widget_blueprint`、`add_text_block_to_widget`、`add_button_to_widget`、`bind_widget_event`、`set_text_block_binding`、`add_widget_to_viewport` |
| DataTable | `create_data_table`、`add_row`、`read_row` |
| 材质 | `create_material`、`add_vector_parameter`、`set_material_on_component` |
| 其它 | `create_input_mapping`、`get_output_log` |

## 自动化测试

在编辑器内可用：

```
Automation RunTests UnrealMCP
```

覆盖：JSON 分帧与上限、真实 socket 收发与背压、UTF-8 字节长度、资产路径校验、落盘与重载持久化、
属性写入与非法输入拒绝、Schema 连线校验、UMG 控件与事件绑定、停止／重启生命周期。

## 许可

MIT。上游代码版权归 [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp) 作者所有；
本分支的修改部分版权归本仓库维护者。详见 [LICENSE](./LICENSE) 与 [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md)。
