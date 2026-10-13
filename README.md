# UnrealMCP — UE4.26 fork

[中文](README.zh-CN.md) | English

A modified version of [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp), based on upstream commit `4e5f00da5073`. This independent fork includes the C++ editor plugin and its Python MCP server in one repository. Maintained by [springkai66](https://github.com/springkai66); upstream originally targets UE5.

```text
AI client → MCP stdio → Python server → TCP 127.0.0.1:55557 → Unreal editor plugin
```

## Main changes

- UE4.26 editor compatibility; corrected Blueprint/UMG creation, reflection property writes and DataTable row handling.
- JSON byte framing, UTF-8 fragmentation, partial sends, socket cleanup and shutdown handling.
- Asset path validation, save/compile error reporting and graph schema connection checks.
- UMG text/button/event authoring, materials and output-log tools; Python exposes 35 tools.
- Python response unwrapping, tool registration, connection handling, shared script transport and regression checks. Project-specific paths and names removed from the release copy.

## Enable the plugin

Requirements: **UE4.26.2 on Windows**, a C++ toolchain for that engine, **Python 3.10+** and [uv](https://docs.astral.sh/uv/getting-started/installation/).

1. Copy this repository into `<YourProject>/Plugins/UnrealMCP`, so `UnrealMCP.uplugin` is directly inside that folder. For a Blueprint-only project, add a C++ class first to enable project compilation.
2. Generate project files and build your project's Editor target using the matching UE4 toolchain.
3. In **Edit → Plugins**, enable **Editor Scripting Utilities** and **UnrealMCP**, then restart the editor.
4. Check Output Log for `UnrealMCPBridge: Server started on 127.0.0.1:55557`. Keep the editor open when calling tools; only one editor can use this port at a time.
5. Install Python dependencies once:

   ```sh
   uv --directory "C:/path/to/unreal-mcp/Python" sync --frozen
   ```

Replace the example directory with the **absolute path to `Python/`**, either in the checkout or installed plugin. Replace `uv` with its executable's absolute path if a desktop client cannot find it. Merge the following entries into existing client settings.

## Connect an MCP client

The client launches the Python server over **stdio**; port `55557` is the plugin's raw TCP endpoint, not an HTTP/SSE MCP URL. No separate server launch or API key is required by this bridge.

### Claude Code

```sh
claude mcp add --transport stdio --scope user unreal-mcp -- uv --directory "C:/path/to/unreal-mcp/Python" run --frozen unreal_mcp_server.py
```

Use `/mcp` to check its status. For project scope, use `--scope project` or the JSON below in `.mcp.json`.

### Pi, WorkBuddy and ZCode

Use this shared JSON:

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

| Client | Where to add it |
| --- | --- |
| **Pi** | `~/.pi/agent/mcp.json` or project `.pi/mcp.json`; check with `/mcp`. For desktop distributions, use their active MCP configuration directory. |
| **WorkBuddy** (Tencent desktop) | Sidebar **Plugins → MCP Servers → Configure MCP** (Chinese labels: 插件 → MCP 服务器 → 配置 MCP). Save to `~/.workbuddy/mcp.json` or project `.workbuddy/mcp.json`. |
| **ZCode** (Z.ai desktop) | **Settings → MCP Servers → New MCP Server → Full configuration**; select User/Workspace, paste JSON, add and enable. This is UI input; native `.zcode` files use `mcp.servers`. |

`~` means your home directory; on Windows it is usually `%USERPROFILE%`.

### Codex

Add to `~/.codex/config.toml` (or trusted-project `.codex/config.toml`):

```toml
[mcp_servers.unreal-mcp]
command = "uv"
args = ["--directory", "C:/path/to/unreal-mcp/Python", "run", "--frozen", "unreal_mcp_server.py"]
```

Alternatively: `codex mcp add unreal-mcp -- uv --directory "C:/path/to/unreal-mcp/Python" run --frozen unreal_mcp_server.py`. Check with `codex mcp list`.

### OpenCode

Merge into project `opencode.json` or `~/.config/opencode/opencode.json`:

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

Check with `opencode mcp list`. After connecting any client, ask: **“Use unreal-mcp to list actors in the current level.”** Tool discovery can succeed before the editor is running; actual editor calls require the plugin listener.

Configuration references: [Claude Code](https://code.claude.com/docs/en/mcp), [Codex](https://developers.openai.com/codex/mcp/), [Pi](https://github.com/earendil-works/pi/blob/main/packages/coding-agent/docs/mcp.md), [OpenCode](https://opencode.ai/docs/mcp-servers/), [WorkBuddy](https://www.codebuddy.ai/docs/zh/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/MCP-Guide), [ZCode](https://zcode.z.ai/en/docs/mcp-services).

## Blueprint layout rule

The plugin arranges graph layout itself: nodes created without an explicit position are placed automatically, and `compile_blueprint` formats this session's generated nodes before compiling. Use `get_blueprint_graph` to read geometry and `format_blueprint_graph` (explicit `node_ids` or `whole_graph=true`) to tidy existing graphs. Layout moves nodes only; connections and behavior are preserved, cycles and shared nodes are kept with warnings, and no zero-crossing guarantee is made.

For UMG, send the AI a sketch or confirm a textual layout first. The plugin exposes `list_widget_blueprints`, `get_widget_tree` and `add_widget_to_widget` for native and project widgets with validated Canvas/Box/Overlay/Border/SizeBox slot layouts. AI instructions still ask for aligned, separated flows; the plugin now enforces the mechanical parts.

## Validation and limits

UE4.26.2 Win64 plugin compilation and 12 isolated-host automation tests passed (including graph layout and UMG composition suites). Python has 31 passing local tests, including stdio calls to a fake TCP editor. The six client configurations follow their documentation; they have not all been tested in the actual apps. UE5, Mac/Linux, runtime UMG rendering and editor-restart persistence are not fully verified.

Known risk: an automation run in the original host project exited with heap corruption (0xC0000374) before this release. Isolated-host tests pass and cannot attribute that exit to any cause; the new code paths here (off-screen Slate node measurement, editor-object flag handling) have NOT been ruled out as contributors, so treat the plugin as unproven in that environment until a run there completes cleanly.

Clearing `RF_ArchetypeObject` on the source widget tree follows engine `UWidgetBlueprint::PostLoad` behavior for freshly created Blueprints; for assets created by the editor UI this flag is normally already cleared, so the change is a no-op there, but saving after these commands persists the cleared flag into the asset.

```sh
uv --directory "C:/path/to/unreal-mcp/Python" run --frozen python -m unittest discover -s tests
```

In Unreal's console: `Automation RunTests UnrealMCP`. Manual scripts under `Python/scripts/` create or modify editor content; run them deliberately. Python-specific API migration notes are in [Python/README.md](Python/README.md).

## License

MIT; attribution and upstream license declaration are documented in [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
