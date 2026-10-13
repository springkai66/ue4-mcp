"""UnrealMCP 客户端回归测试。

用假 Unreal（脚本化响应）驱动真实的 MCP 工具入口，覆盖参数形状、
响应解包与错误上报；不需要启动 UE 编辑器。

运行：.venv/Scripts/python.exe -m unittest discover -s tests -v
"""

import json
import os
import sys
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import unreal_mcp_server as srv


class FakeConnection:
    """记录发往 Unreal 的命令，并按脚本返回响应。"""

    def __init__(self, responder=None):
        self.calls = []
        self.responder = responder
        self.connected = True

    def send_command(self, command, params):
        self.calls.append((command, params))
        if self.responder:
            return self.responder(command, params)
        return {"status": "success", "result": {}}


def tool_fn(name):
    """取回已注册工具的实现函数（这些工具都不使用 ctx）。"""
    for t in srv.mcp._tool_manager.list_tools():
        if t.name == name:
            return t.fn
    raise AssertionError(f"tool not registered: {name}")


def call_tool(name, arguments):
    """直接调用工具实现，不经过 MCP 序列化层。"""
    return tool_fn(name)(ctx=None, **arguments)


class ToolRegistrationTests(unittest.TestCase):
    """契约：插件支持的每一类命令都必须有可达的 MCP 工具。"""

    EXPECTED = {
        # material_tools + debug_tools 必须与其它模块一起注册
        "create_material",
        "add_vector_parameter",
        "set_material_on_component",
        "get_output_log",
    }

    def test_material_and_debug_tools_are_registered(self):
        tools = {t.name for t in srv.mcp._tool_manager.list_tools()}
        missing = sorted(self.EXPECTED - tools)
        self.assertEqual([], missing, f"工具未注册: {missing}")

    def test_tool_count_does_not_regress(self):
        # 重构不得减少已暴露的工具数量
        tools = {t.name for t in srv.mcp._tool_manager.list_tools()}
        self.assertGreaterEqual(len(tools), 35)


class ResponseUnwrapTests(unittest.TestCase):
    """契约：插件把负载包在 result 内，工具必须按内层取值。"""

    def test_find_actors_by_name_reads_nested_actors(self):
        fake = FakeConnection(lambda c, p: {
            "status": "success",
            "result": {"actors": ["Alpha", "Beta"]},
        })
        with mock.patch.object(srv, "_unreal_connection", fake):
            actors = call_tool("find_actors_by_name", {"pattern": "A"})
        self.assertEqual(["Alpha", "Beta"], actors)

    def test_get_actors_in_level_reads_nested_actors(self):
        fake = FakeConnection(lambda c, p: {
            "status": "success",
            "result": {"actors": [{"name": "A"}]},
        })
        with mock.patch.object(srv, "_unreal_connection", fake):
            self.assertEqual([{"name": "A"}], call_tool("get_actors_in_level", {}))

    def test_result_payload_helper_handles_missing_and_odd_shapes(self):
        self.assertEqual({}, srv.result_payload(None))
        self.assertEqual({}, srv.result_payload({"status": "error"}))
        self.assertEqual({}, srv.result_payload({"result": "not-a-dict"}))
        self.assertEqual({"a": 1}, srv.result_payload({"result": {"a": 1}}))


class CommandShapeTests(unittest.TestCase):
    """契约：发往插件的命令名与参数名必须与插件读取的字段一致。"""

    def capture(self, tool, arguments, responder=None):
        fake = FakeConnection(responder)
        with mock.patch.object(srv, "_unreal_connection", fake):
            call_tool(tool, arguments)
        return fake.calls

    def test_spawn_actor_preserves_public_type_parameter(self):
        """保留既有工具参数名，并使用插件接受的字段。"""
        calls = self.capture("spawn_actor", {"name": "N", "type": "StaticMeshActor"})
        command, params = calls[0]
        self.assertEqual("spawn_actor", command)
        self.assertEqual("StaticMeshActor", params["type"])
        self.assertNotIn("actor_type", params)

    def test_create_input_mapping_sends_input_type(self):
        calls = self.capture("create_input_mapping",
                             {"action_name": "A", "key": "F", "input_type": "Axis"})
        self.assertEqual("create_input_mapping", calls[0][0])
        self.assertEqual("Axis", calls[0][1]["input_type"])

    def test_input_mapping_rejects_unknown_type_before_sending(self):
        fake = FakeConnection()
        with mock.patch.object(srv, "_unreal_connection", fake):
            response = call_tool("create_input_mapping", {"action_name": "A", "key": "F", "input_type": "Axix"})
        self.assertFalse(response["success"])
        self.assertEqual([], fake.calls)
        calls = self.capture("create_input_mapping", {"action_name": "A", "key": "F", "input_type": "axis"})
        self.assertEqual("axis", calls[0][1]["input_type"])

    def test_bind_widget_event_does_not_send_function_name(self):
        """插件不读 function_name；继续发送会造成已文档化但被忽略的参数。"""
        calls = self.capture("bind_widget_event", {
            "widget_name": "W", "widget_component_name": "B", "event_name": "OnClicked",
        })
        _, params = calls[0]
        self.assertNotIn("function_name", params)

    def test_set_physics_properties_does_not_send_gravity_enabled(self):
        """插件不读 gravity_enabled；该参数是文档承诺但实现不接受的输入。"""
        calls = self.capture("set_physics_properties",
                            {"blueprint_name": "B", "component_name": "C"})
        _, params = calls[0]
        self.assertNotIn("gravity_enabled", params)

    def test_add_component_to_blueprint_does_not_send_component_properties(self):
        """插件不读 component_properties；静默忽略的参数不应继续宣传。"""
        calls = self.capture("add_component_to_blueprint",
                            {"blueprint_name": "B", "component_type": "StaticMeshComponent",
                             "component_name": "C"})
        _, params = calls[0]
        self.assertNotIn("component_properties", params)


class PromptAccuracyTests(unittest.TestCase):
    """契约：info 提示词只能宣传真实存在的工具。"""

    def test_info_prompt_only_names_registered_tools(self):
        import re

        registered = {t.name for t in srv.mcp._tool_manager.list_tools()}
        prompt = srv.info()
        # 提示词里用 `tool_name(...)` 形式列举工具
        named = set(re.findall(r"`(\w+)\(", prompt))
        unknown = sorted(named - registered)
        self.assertEqual([], unknown, f"提示词宣传了不存在的工具: {unknown}")
        import inspect
        for name, parameters in re.findall(r"`(\w+)\(([^)]*)\)`", prompt):
            signature = inspect.signature(tool_fn(name))
            parameters = re.sub(r"\[[^]]*\]", "[]", parameters)
            advertised = [part.strip().split("=", 1)[0] for part in parameters.split(",") if part.strip()]
            for parameter in advertised:
                self.assertIn(parameter, signature.parameters, f"{name}: {parameter}")


class ErrorReportingTests(unittest.TestCase):
    """契约：连接不可用必须可被调用方识别，而不是伪装成空成功。"""

    def test_no_connection_result_is_recognisable(self):
        """连接失败必须返回可识别的失败结果，而不是空成功。"""
        result = srv.no_connection()
        self.assertFalse(result["success"])
        self.assertIn("connect", result["message"].lower())

    def test_get_unreal_connection_returns_none_when_connect_fails(self):
        with mock.patch.object(srv, "_unreal_connection", None), \
             mock.patch.object(srv.UnrealConnection, "connect", return_value=False):
            self.assertIsNone(srv.get_unreal_connection())

    def test_every_tool_reports_failure_when_editor_is_down(self):
        """编辑器不可用时，每个工具都必须返回失败而不是 NameError/异常。

        验证返回结果，避免异常被工具捕获后漏报。
        """
        # 各工具的必填参数（仅用于让调用走到连接检查）
        required = {
            "spawn_actor": {"name": "N", "type": "StaticMeshActor"},
            "create_blueprint": {"name": "N", "parent_class": "Actor"},
            "add_component_to_blueprint": {"blueprint_name": "B", "component_type": "SphereComponent", "component_name": "C"},
            "set_static_mesh_properties": {"blueprint_name": "B", "component_name": "C"},
            "set_component_property": {"blueprint_name": "B", "component_name": "C", "property_name": "P", "property_value": "V"},
            "set_physics_properties": {"blueprint_name": "B", "component_name": "C"},
            "compile_blueprint": {"blueprint_name": "B"},
            "set_blueprint_property": {"blueprint_name": "B", "property_name": "P", "property_value": "V"},
            "add_blueprint_event_node": {"blueprint_name": "B", "event_name": "ReceiveBeginPlay"},
            "add_blueprint_input_action_node": {"blueprint_name": "B", "action_name": "A"},
            "add_blueprint_function_node": {"blueprint_name": "B", "target": "T", "function_name": "F"},
            "connect_blueprint_nodes": {"blueprint_name": "B", "source_node_id": "S", "source_pin": "P", "target_node_id": "T", "target_pin": "Q"},
            "add_blueprint_variable": {"blueprint_name": "B", "variable_name": "V", "variable_type": "Boolean"},
            "add_blueprint_get_self_component_reference": {"blueprint_name": "B", "component_name": "C"},
            "add_blueprint_self_reference": {"blueprint_name": "B"},
            "find_blueprint_nodes": {"blueprint_name": "B", "node_type": "Event"},
            "create_input_mapping": {"action_name": "A", "key": "F"},
            "create_umg_widget_blueprint": {"widget_name": "W"},
            "add_text_block_to_widget": {"widget_name": "W", "text_block_name": "T"},
            "add_button_to_widget": {"widget_name": "W", "button_name": "B"},
            "bind_widget_event": {"widget_name": "W", "widget_component_name": "C", "event_name": "OnClicked"},
            "add_widget_to_viewport": {"widget_name": "W"},
            "get_blueprint_graph": {"blueprint_name": "B"},
            "format_blueprint_graph": {"blueprint_name": "B", "node_ids": []},
            "list_widget_blueprints": {},
            "get_widget_tree": {"widget_name": "W"},
            "add_widget_to_widget": {"widget_name": "W", "child_name": "C", "widget_class": "TextBlock"},
            "set_text_block_binding": {"widget_name": "W", "text_block_name": "T", "binding_property": "P"},
            "create_material": {"material_name": "M"},
            "add_vector_parameter": {"material_name": "M", "param_name": "P"},
            "set_material_on_component": {"actor_name": "A", "component_name": "C", "material_path": "/Game/M.M"},
            "get_output_log": {},
            "get_actors_in_level": {},
            "find_actors_by_name": {"pattern": "A"},
            "delete_actor": {"name": "A"},
            "set_actor_transform": {"name": "A"},
            "get_actor_properties": {"name": "A"},
            "set_actor_property": {"name": "A", "property_name": "P", "property_value": "V"},
            "spawn_blueprint_actor": {"blueprint_name": "B", "actor_name": "A"},
            "duplicate_level": {"source_path": "/Game/A", "destination_path": "/Game/B"},
        }
        tools = {t.name: t for t in srv.mcp._tool_manager.list_tools()}
        with mock.patch.object(srv, "_unreal_connection", None), \
             mock.patch.object(srv.UnrealConnection, "connect", return_value=False):
            for name in sorted(tools):
                args = required.get(name)
                if args is None:
                    self.fail(f"工具 {name} 未在测试中覆盖，请补充必填参数")
                with self.subTest(tool=name):
                    result = tools[name].fn(ctx=None, **args)
                    if name in {"get_actors_in_level", "find_actors_by_name"}:
                        self.assertEqual([], result)
                    else:
                        self.assertEqual({"success": False,
                                          "message": "Failed to connect to Unreal Engine"}, result)

    def test_editor_dict_tools_report_no_response_and_exception(self):
        cases = {
            "delete_actor": {"name": "A"},
            "set_actor_transform": {"name": "A"},
            "get_actor_properties": {"name": "A"},
            "duplicate_level": {"source_path": "/Game/A", "destination_path": "/Game/B"},
        }
        for name, arguments in cases.items():
            with self.subTest(tool=name), mock.patch.object(srv, "_unreal_connection", FakeConnection(lambda c, p: None)):
                self.assertEqual({"success": False, "message": "No response from Unreal Engine"},
                                 call_tool(name, arguments))
            def fail(command, params):
                raise ConnectionError("test transport failure")
            with self.subTest(tool=name), mock.patch.object(srv, "_unreal_connection", FakeConnection(fail)):
                response = call_tool(name, arguments)
                self.assertFalse(response["success"])
                self.assertIn("test transport failure", response["message"])

    def test_get_unreal_connection_has_no_socket_probe(self):
        """连接探针曾以 b'\\x00' 做假 ping，会破坏协议；不得再出现。"""
        server = os.path.join(os.path.dirname(os.path.dirname(
            os.path.abspath(__file__))), "unreal_mcp_server.py")
        with open(server, encoding="utf-8") as handle:
            source = handle.read()
        self.assertNotIn("ping test", source)
        self.assertNotIn("sendall(b'\\x00')", source)


if __name__ == "__main__":
    unittest.main()
