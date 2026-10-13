"""蓝图图表查询与局部排版工具。"""
from typing import Any, Dict, List, Optional
from mcp.server.fastmcp import Context, FastMCP


def register_layout_tools(mcp: FastMCP):
    @mcp.tool()
    def get_blueprint_graph(ctx: Context, blueprint_name: str) -> Dict[str, Any]:
        """Read the first Event Graph: node IDs, positions, measured sizes and pin links.

        Does not create a graph. size_source distinguishes Slate desired size
        from fallback estimates; neither is a screenshot inspection.
        """
        from unreal_mcp_server import get_unreal_connection, no_connection
        unreal = get_unreal_connection()
        if not unreal:
            return no_connection()
        return unreal.send_command("get_blueprint_graph", {"blueprint_name": blueprint_name}) or {
            "success": False, "message": "No response from Unreal Engine"}

    @mcp.tool()
    def format_blueprint_graph(ctx: Context, blueprint_name: str,
                               node_ids: Optional[List[str]] = None,
                               whole_graph: bool = False) -> Dict[str, Any]:
        """Arrange the first Event Graph without changing nodes or connections.

        Supply explicit node_ids for a local selection, or whole_graph=True
        to allow moving all non-comment nodes. Omitted/empty selection never
        means whole graph. Nodes outside a selection stay fixed. Cycles and
        shared execution nodes are retained with warnings; no zero-crossing
        guarantee. Explicitly selecting a node allows moving it even if it
        was originally created with a position.
        """
        from unreal_mcp_server import get_unreal_connection, no_connection
        if whole_graph == (node_ids is not None):
            return {"success": False, "message": "Choose node_ids or whole_graph=True"}
        unreal = get_unreal_connection()
        if not unreal:
            return no_connection()
        params: Dict[str, Any] = {"blueprint_name": blueprint_name}
        if whole_graph:
            params["whole_graph"] = True
        else:
            params["node_ids"] = node_ids
        return unreal.send_command("format_blueprint_graph", params) or {
            "success": False, "message": "No response from Unreal Engine"}
