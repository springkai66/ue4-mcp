"""真实 stdio 子进程的初始化与工具发现检查，不调用编辑器。"""

import asyncio
import json
from pathlib import Path
import sys
import unittest

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client


class StdioTests(unittest.IsolatedAsyncioTestCase):
    async def test_initialize_and_list_tools(self):
        root = Path(__file__).resolve().parent.parent
        parameters = StdioServerParameters(command=sys.executable,
                                           args=[str(root / "unreal_mcp_server.py")],
                                           cwd=str(root))

        async def check():
            async with stdio_client(parameters) as (reader, writer):
                async with ClientSession(reader, writer) as session:
                    initialization = await session.initialize()
                    # 验证规范随 MCP 握手传给客户端，不依赖主动请求提示词
                    self.assertIn("Blueprint layout requirements", initialization.instructions)
                    self.assertIn("align each execution chain horizontally", initialization.instructions)
                    result = await session.list_tools()
                    tools = {tool.name: tool for tool in result.tools}
                    self.assertEqual(40, len(tools))
                    self.assertIn("type", tools["spawn_actor"].inputSchema["properties"])
                    self.assertIn("get_output_log", tools)
                    self.assertIn("create_material", tools)
                    self.assertIn("separate unrelated", tools["connect_blueprint_nodes"].description)
        await asyncio.wait_for(check(), timeout=15)

    async def test_tool_call_roundtrip_to_fake_tcp_editor(self):
        requests = []
        async def handle(reader, writer):
            try:
                request = json.loads(await reader.read(4096))
                requests.append(request)
                wire = json.dumps({"status": "success", "result": {
                    "actors": ["Alpha"]}}, ensure_ascii=False).encode()
                writer.write(wire)
                await writer.drain()
            finally:
                writer.close()
                await writer.wait_closed()

        root = Path(__file__).resolve().parent.parent
        server = await asyncio.start_server(handle, "127.0.0.1", 0)
        port = server.sockets[0].getsockname()[1]
        parameters = StdioServerParameters(command=sys.executable, args=["-c",
            f"import unreal_mcp_server as s; s.UNREAL_PORT={port}; s.mcp.run(transport='stdio')"],
            cwd=str(root))
        async def check():
            async with server, stdio_client(parameters) as (reader, writer):
                async with ClientSession(reader, writer) as session:
                    await session.initialize()
                    for _ in range(2):
                        result = await session.call_tool("find_actors_by_name", {"pattern": "A"})
                        self.assertFalse(result.isError)
                        self.assertIn("Alpha", "".join(item.text for item in result.content))
        await asyncio.wait_for(check(), timeout=15)
        self.assertEqual([{"type": "find_actors_by_name", "params": {"pattern": "A"}}] * 2, requests)


if __name__ == "__main__":
    unittest.main()
