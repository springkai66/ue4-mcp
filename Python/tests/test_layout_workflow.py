"""布局工作流通过真实工具入口与传输接口验证，不操作编辑器。"""
import unittest
from unittest import mock
import unreal_mcp_server as srv


class LayoutWorkflowTests(unittest.TestCase):
    def test_compile_formats_only_pending_nodes_of_that_blueprint(self):
        connection = srv.UnrealConnection()
        calls = []
        def reply(command, params=None):
            calls.append((command, params))
            if command == "add_blueprint_self_reference":
                return {"status": "success", "result": {"node_id": params["blueprint_name"] + "-node"}}
            return {"status": "success", "result": {}}
        with mock.patch.object(connection, "_send_command", side_effect=reply):
            connection.send_command("add_blueprint_self_reference", {"blueprint_name": "A"})
            connection.send_command("add_blueprint_self_reference", {"blueprint_name": "B"})
            connection.send_command("compile_blueprint", {"blueprint_name": "A"})
        self.assertEqual(("format_blueprint_graph", {"blueprint_name": "A", "node_ids": ["A-node"]}), calls[-2])
        self.assertEqual(["B-node"], connection.pending_layout["B"])
        self.assertNotIn("A", connection.pending_layout)

    def test_layout_failure_stops_compile_and_keeps_retry_selection(self):
        connection = srv.UnrealConnection()
        connection.pending_layout["A"] = ["node"]
        with mock.patch.object(connection, "_send_command", return_value={"status": "error", "error": "layout failed"}) as send:
            response = connection.send_command("compile_blueprint", {"blueprint_name": "A"})
        self.assertEqual("error", response["status"])
        self.assertEqual(["node"], connection.pending_layout["A"])
        self.assertEqual(1, send.call_count)

    def test_explicit_positions_do_not_enter_automatic_selection(self):
        connection = srv.UnrealConnection()
        with mock.patch.object(connection, "_send_command", return_value={"status": "success", "result": {"node_id": "fixed"}}):
            connection.send_command("add_blueprint_self_reference", {"blueprint_name": "A", "node_position": [100, 200]})
        self.assertEqual({}, connection.pending_layout)


if __name__ == "__main__":
    unittest.main()
