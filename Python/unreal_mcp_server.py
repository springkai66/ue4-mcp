"""
Unreal Engine MCP Server

A simple MCP server for interacting with Unreal Engine.
"""

import logging
import socket
import sys
import json
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional
from mcp.server.fastmcp import FastMCP
from unreal_transport import receive_response

# Configure logging with more detailed format
logging.basicConfig(
    level=logging.DEBUG,  # Change to DEBUG level for more details
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[
        logging.FileHandler('unreal_mcp.log'),
        # logging.StreamHandler(sys.stdout) # Remove this handler to unexpected non-whitespace characters in JSON
    ]
)
logger = logging.getLogger("UnrealMCP")

# Configuration
UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557

class UnrealConnection:
    """Connection to an Unreal Engine instance."""
    
    def __init__(self):
        """Initialize the connection."""
        self.socket = None
        self.connected = False
        self.pending_layout: Dict[str, list[str]] = {}
    
    def connect(self) -> bool:
        """Connect to the Unreal Engine instance."""
        try:
            # Close any existing socket
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
            
            logger.info(f"Connecting to Unreal at {UNREAL_HOST}:{UNREAL_PORT}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(30)  # 30s: some UE ops (compile/asset load) exceed 5s
            
            # Set socket options for better stability
            self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            
            # Set larger buffer sizes
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
            
            self.socket.connect((UNREAL_HOST, UNREAL_PORT))
            self.connected = True
            logger.info("Connected to Unreal Engine")
            return True
            
        except Exception as e:
            logger.error(f"Failed to connect to Unreal: {e}")
            self.disconnect()
            return False
    
    def disconnect(self):
        """Disconnect from the Unreal Engine instance."""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.socket = None
        self.connected = False

    def receive_full_response(self, sock, buffer_size=4096) -> bytes:
        """接收完整响应，使用服务和脚本共用的字节接收逻辑。"""
        return receive_response(sock, buffer_size)

    def send_command(self, command: str, params: Optional[Dict[str, Any]] = None) -> Optional[Dict[str, Any]]:
        """生成结束编译前，仅整理本轮未显式定位的节点。"""
        parameters = params or {}
        blueprint = parameters.get("blueprint_name", "")
        if command == "compile_blueprint" and blueprint in self.pending_layout:
            selection = list(self.pending_layout[blueprint])
            layout = self._send_command("format_blueprint_graph", {"blueprint_name": blueprint, "node_ids": selection})
            if not layout or layout.get("status") != "success":
                return layout or {"status": "error", "error": "No response while formatting graph"}
            del self.pending_layout[blueprint]
        response = self._send_command(command, parameters)
        if response and response.get("status") == "success":
            node_commands = {"add_blueprint_event_node", "add_blueprint_function_node",
                             "add_blueprint_input_action_node", "add_blueprint_self_reference",
                             "add_blueprint_get_self_component_reference", "add_blueprint_cast_node",
                             "add_blueprint_external_member_get_node"}
            if command in node_commands and "node_position" not in parameters and not result_payload(response).get("already_exists"):
                node_id = result_payload(response).get("node_id")
                if isinstance(node_id, str) and blueprint:
                    ids = self.pending_layout.setdefault(blueprint, [])
                    if node_id not in ids:
                        ids.append(node_id)
            if command == "format_blueprint_graph":
                ids = parameters.get("node_ids", [])
                if parameters.get("whole_graph"):
                    self.pending_layout.pop(blueprint, None)
                elif blueprint in self.pending_layout:
                    remaining = [node for node in self.pending_layout[blueprint] if node not in ids]
                    if remaining:
                        self.pending_layout[blueprint] = remaining
                    else:
                        del self.pending_layout[blueprint]
        return response

    def _send_command(self, command: str, params: Optional[Dict[str, Any]] = None) -> Optional[Dict[str, Any]]:
        """发送一次命令；失败后不重发可能已经执行的编辑器操作。"""
        if not self.connected:
            if not self.connect():
                return None
        try:
            sock = self.socket
            if sock is None:
                raise ConnectionError("Unreal socket is not connected")
            command_json = json.dumps({"type": command, "params": params or {}})
            sock.sendall(command_json.encode("utf-8"))
            response = json.loads(self.receive_full_response(sock))
            if response.get("status") == "error":
                response.setdefault("error", response.get("message", "Unknown Unreal error"))
            elif response.get("success") is False:
                response = {"status": "error", "error": response.get("error") or
                            response.get("message", "Unknown Unreal error")}
            return response
        except Exception as error:
            logger.error(f"Error sending command: {error}")
            return {"status": "error", "error": str(error)}
        finally:
            self.disconnect()

# Global connection state
_unreal_connection: Optional[UnrealConnection] = None

def get_unreal_connection() -> Optional[UnrealConnection]:
    """Get the connection to Unreal Engine."""
    global _unreal_connection
    try:
        if _unreal_connection is None:
            _unreal_connection = UnrealConnection()
        if not _unreal_connection.connected:
            if not _unreal_connection.connect():
                logger.warning("Could not connect to Unreal Engine")
                return None

        return _unreal_connection
    except Exception as e:
        logger.error(f"Error getting Unreal connection: {e}")
        return None

def no_connection() -> Dict[str, Any]:
    """Log and return the standard "could not connect" result.

    Kept separate from the "no response" result, because callers distinguish
    "the editor is not reachable" from "the command returned nothing".
    """
    logger.error("Failed to connect to Unreal Engine")
    return {"success": False, "message": "Failed to connect to Unreal Engine"}

def result_payload(response: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    """Return the inner payload of a bridged response.

    The plugin wraps every reply as {"status": ..., "result": {...}}, so a
    tool that needs a field from the payload must unwrap it first.
    """
    if not response:
        return {}
    payload = response.get("result")
    return payload if isinstance(payload, dict) else {}

@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    """Handle server startup and shutdown."""
    global _unreal_connection
    logger.info("UnrealMCP server starting up")
    # 首个工具调用时再建立连接，避免启动阶段留下空闲或失效连接
    try:
        yield {}
    finally:
        if _unreal_connection:
            _unreal_connection.disconnect()
            _unreal_connection = None
        logger.info("Unreal MCP server shut down")

# 连接初始化和 info 提示词共用同一份蓝图整洁规范
BLUEPRINT_LAYOUT_RULES = """Blueprint layout requirements:
- Plan positions before creating nodes. Use node_position where supported; arrange execution left to right, align each execution chain horizontally, and keep consistent spacing on the editor grid.
- Place each event or function flow in its own area, with related data nodes close to their consumers. Avoid overlapping nodes, crossing wires, backward wires and long wires between separate flows.
- Do not funnel unrelated event/function execution chains into the same operation node merely to reuse it. Use separate call nodes for separate flows; when logic genuinely belongs together, prefer a shared function with a separate call node in each flow if supported.
- Preserve valid branch convergence within a related flow, shared data nodes and execution semantics. Place branches on separate rows; never split or duplicate stateful nodes or introduce redundant logic solely for layout.
- compile_blueprint automatically formats nodes created without explicit positions in this server session. For existing graphs, inspect get_blueprint_graph and use format_blueprint_graph with explicit node_ids or whole_graph=true. Report measured/estimated geometry separately from visual inspection.
UMG design workflow:
- Before creating a complete interface, ask for a sketch/screenshot and target resolution. A sketch is preferred, not mandatory: if unavailable, confirm a textual layout plan before creating widgets. Do not repeatedly ask for a sketch for a single widget or a small edit.
- Honor user-specified widget types first. Otherwise inspect list_widget_blueprints and get_widget_tree, recommend project controls based on observed usage and ask the user when candidates are ambiguous. Never infer popularity from names alone.
- Interpret images in the AI client and send explicit layout data to the plugin. Use appropriate Canvas, HorizontalBox/VerticalBox, Overlay, Border or SizeBox containers and anchors instead of stacking all widgets at absolute coordinates.
"""

# Initialize server
mcp = FastMCP(
    "UnrealMCP",
    instructions="Unreal Engine integration via Model Context Protocol.\n" + BLUEPRINT_LAYOUT_RULES,
    lifespan=server_lifespan
)

# 以脚本启动时让工具的延迟导入复用同一模块及连接状态
if __name__ == "__main__":
    sys.modules["unreal_mcp_server"] = sys.modules[__name__]

# Import and register tools
from tools.editor_tools import register_editor_tools
from tools.blueprint_tools import register_blueprint_tools
from tools.node_tools import register_blueprint_node_tools
from tools.project_tools import register_project_tools
from tools.umg_tools import register_umg_tools
from tools.material_tools import register_material_tools
from tools.debug_tools import register_debug_tools
from tools.layout_tools import register_layout_tools

# Register tools
register_editor_tools(mcp)
register_blueprint_tools(mcp)
register_blueprint_node_tools(mcp)
register_project_tools(mcp)
register_umg_tools(mcp)
register_material_tools(mcp)
register_debug_tools(mcp)
register_layout_tools(mcp)

@mcp.prompt()
def info():
    """Information about available Unreal MCP tools and best practices."""
    return BLUEPRINT_LAYOUT_RULES + """
    # Unreal MCP Server Tools and Best Practices
    
    ## UMG (Widget Blueprint) Tools
    - `create_umg_widget_blueprint(widget_name, parent_class="UserWidget", path="/Game/UI")` 
      Create a new UMG Widget Blueprint
    - `add_text_block_to_widget(widget_name, text_block_name, text="", position=[0,0], size=[200,50], font_size=12, color=[1,1,1,1])`
      Add a Text Block widget with customizable properties
    - `add_button_to_widget(widget_name, button_name, text="", position=[0,0], size=[200,50], font_size=12, color=[1,1,1,1], background_color=[0.1,0.1,0.1,1])`
      Add a Button widget with text and styling
    - `bind_widget_event(widget_name, widget_component_name, event_name)`
      Bind a widget event (e.g. OnClicked) to a component-bound event node
    - `add_widget_to_viewport(widget_name, z_order=0)`
      Add widget instance to game viewport
    - `set_text_block_binding(widget_name, text_block_name, binding_property, binding_type="Text")`
      Set up dynamic text binding (only Text bindings are supported)

    - `list_widget_blueprints(path="/Game", recursive=true)` - List project Widget Blueprints
    - `get_widget_tree(widget_name)` - Read a widget hierarchy and slot layouts
    - `add_widget_to_widget(widget_name, child_name, widget_class, parent_name="", layout={}, properties={})` - Add native or project widgets to any panel

    ## Editor Tools
    ### Actor Management
    - `get_actors_in_level()` - List all actors in current level
    - `find_actors_by_name(pattern)` - Find actor names by pattern
    - `spawn_actor(name, type, location=[0,0,0], rotation=[0,0,0], scale=[1,1,1], mesh_path="")` - Create actors
    - `delete_actor(name)` - Remove actors
    - `set_actor_transform(name, location, rotation, scale)` - Modify actor transform
    - `get_actor_properties(name)` - Get actor properties
    - `set_actor_property(name, property_name, property_value)` - Set an actor property
    - `spawn_blueprint_actor(blueprint_name, actor_name)` - Spawn a Blueprint actor
    - `duplicate_level(source_path, destination_path)` - Duplicate a level
    
    ## Blueprint Management
    - `create_blueprint(name, parent_class)` - Create new Blueprint classes
    - `add_component_to_blueprint(blueprint_name, component_type, component_name)` - Add components
    - `set_static_mesh_properties(blueprint_name, component_name, static_mesh)` - Configure meshes
    - `set_component_property(blueprint_name, component_name, property_name, property_value)` - Set a component property
    - `set_physics_properties(blueprint_name, component_name, simulate_physics, mass, linear_damping, angular_damping)` - Configure physics
    - `compile_blueprint(blueprint_name)` - Compile Blueprint changes
    - `set_blueprint_property(blueprint_name, property_name, property_value)` - Set properties
    
    ## Blueprint Node Management
    - `get_blueprint_graph(blueprint_name)` - Read node geometry and links
    - `format_blueprint_graph(blueprint_name, node_ids, whole_graph=false)` - Arrange selected nodes or the whole Event Graph
    - `add_blueprint_event_node(blueprint_name, event_name)` - Add event nodes
    - `add_blueprint_input_action_node(blueprint_name, action_name)` - Add input nodes
    - `add_blueprint_function_node(blueprint_name, target, function_name)` - Add function nodes
    - `connect_blueprint_nodes(blueprint_name, source_node_id, source_pin, target_node_id, target_pin)` - Connect nodes
    - `add_blueprint_variable(blueprint_name, variable_name, variable_type)` - Add variables
    - `add_blueprint_get_self_component_reference(blueprint_name, component_name)` - Add component refs
    - `add_blueprint_self_reference(blueprint_name)` - Add self references
    - `find_blueprint_nodes(blueprint_name, node_type, event_type)` - Find nodes
    
    ## Project Tools
    - `create_input_mapping(action_name, key, input_type="Action")` - Create input mappings (Action or Axis)

    ## Material Tools
    - `create_material(material_name, save_path="/Game/Materials")` - Create a Material asset
    - `add_vector_parameter(material_name, param_name, default_color=[r,g,b,a])` - Add a vector parameter and connect it to Base Color
    - `set_material_on_component(actor_name, component_name, material_path, material_index=0)` - Assign a material to a mesh component

    ## Debug Tools
    - `get_output_log(filter="", max_lines=50)` - Read the editor output log
    
    ## Best Practices
    
    ### UMG Widget Development
    - Create widgets with descriptive names that reflect their purpose
    - Use consistent naming conventions for widget components
    - Organize widget hierarchy logically
    - Set appropriate anchors and alignment for responsive layouts
    - Use property bindings for dynamic updates instead of direct setting
    - Handle widget events appropriately with meaningful function names
    - Clean up widgets when no longer needed
    - Test widget layouts at different resolutions
    
    ### Editor and Actor Management
    - Use unique names for actors to avoid conflicts
    - Clean up temporary actors
    - Validate transforms before applying
    - Check actor existence before modifications
    - Take regular viewport screenshots during development
    - Keep the viewport focused on relevant actors during operations
    
    ### Blueprint Development
    - Compile Blueprints after changes
    - Use meaningful names for variables and functions
    - Organize nodes logically
    - Test functionality in isolation
    - Consider performance implications
    - Document complex setups
    
    ### Error Handling
    - Check command responses for success
    - Handle errors gracefully
    - Log important operations
    - Validate parameters
    - Clean up resources on errors
    """

# Run the server
if __name__ == "__main__":
    logger.info("Starting MCP server with stdio transport")
    mcp.run(transport='stdio') 