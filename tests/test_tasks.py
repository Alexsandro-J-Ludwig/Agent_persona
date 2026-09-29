"""Regressões de planejamento, execução em lote, progresso e /verify."""
from io import StringIO
import json
import unittest
from unittest.mock import Mock, patch

import httpx
from ollama import Client, Message
from rich.console import Console

import tools
from agent.agent import Agent
from agent.router import decide_think
from agent.tasks import TaskManager, TASK_TOOL_NAMES


def call(name, **arguments):
    return Message.ToolCall(function={"name": name, "arguments": arguments})


class TaskTests(unittest.TestCase):
    def setUp(self):
        self.output = StringIO()
        self.console = Console(file=self.output, width=120)

    def test_statuses_validated_and_failure_is_a_string(self):
        manager = TaskManager()
        manager.create(["Etapa A", "Etapa B"])
        with self.assertRaises(ValueError):
            manager.create(["Apagar plano"])
        with self.assertRaises(ValueError):
            manager.start(99)
        with self.assertRaises(ValueError):
            manager.complete(1, "Ainda não iniciou")
        manager.start(1)
        with self.assertRaises(ValueError):
            manager.start(2)
        manager.fail(1, "Falha real")
        manager.block(2, "Falta um dado")
        self.assertEqual(manager.tasks[0].status, "failed")
        self.assertFalse(manager.has_pending())
        self.assertIn("Falha real", manager.context())

    def test_batch_executes_all_calls_even_after_invalid_internal_tool(self):
        agent = Agent(Mock(), self.console)
        today = Mock(return_value={"data": "28/09/2026"})
        with patch.dict(tools.AVAILABLE_TOOLS, {"today": today}):
            result = agent._execute_tools([
                call("start_task", task_id=99), call("today"),
                call("create_tasks", tarefas=["Validar datas"]),
                call("start_task", task_id=1),
            ])
        self.assertEqual(len(result), 4)
        self.assertIn("erro", result[0])
        today.assert_called_once()
        self.assertEqual(len(agent.history), 5)
        self.assertEqual(agent.tasks.tasks[0].status, "in_progress")

    def test_empty_response_with_pending_tasks_recovers_and_panel_updates(self):
        agent = Agent(Mock(), self.console)
        responses = [
            ("", "", [call("create_tasks", tarefas=["Conferir a data", "Calcular amanhã"])]),
            ("", "", [call("start_task", task_id=1), call("today")]),
            ("", "", []),  # Reproduz o encerramento prematuro relatado.
            ("", "", [call("complete_task", task_id=1, resultado="Data obtida"), call("start_task", task_id=2), call("calc_data", dias=1, data="28/09/2026")]),
            ("", "", [call("complete_task", task_id=2, resultado="29/09/2026")]),
            ("As duas etapas foram concluídas.", "", []),
        ]
        with patch("agent.agent.stream_model", side_effect=responses) as stream:
            agent.run_agent("Confira a data e calcule amanhã", False)
        self.assertEqual([t.status for t in agent.tasks.tasks], ["completed", "completed"])
        output = self.output.getvalue()
        for text in ("Pendente", "Em andamento", "Concluída", "Conferir a data", "Calcular amanhã"):
            self.assertIn(text, output)
        recovery = stream.call_args_list[3].args[0]
        self.assertIn("[in_progress] Conferir a data", recovery[0]["content"])
        self.assertEqual([m["role"] for m in recovery].count("user"), 1)

    def test_idle_and_round_limit_report_blocked_tasks(self):
        for limit in (1, 5):
            with self.subTest(limit=limit):
                agent = Agent(Mock(), self.console)
                replies = [
                    ("", "", [call("create_tasks", tarefas=["Etapa pendente"])]),
                    ("", "", []), ("", "", []),
                ]
                with patch("agent.agent.MAX_TOOL_ROUNDS", limit), patch("agent.agent.stream_model", side_effect=replies):
                    agent.run_agent("Pedido", False)
                self.assertEqual(agent.tasks.tasks[0].status, "blocked")
                self.assertTrue(agent.tasks.tasks[0].result)

    def test_sdk_receives_task_tools_and_terminal_restores_cursor(self):
        requests = []
        responses = [
            {"content": "", "tool_calls": [call("create_tasks", tarefas=["Etapa visível"]).model_dump(), call("start_task", task_id=1).model_dump()]},
            {"content": "", "tool_calls": [call("complete_task", task_id=1, resultado="Conferido").model_dump()]},
            {"content": "Concluído."},
        ]

        def respond(request):
            requests.append(json.loads(request.content))
            return httpx.Response(200, text=json.dumps({
                "model": "test", "message": {"role": "assistant", **responses.pop(0)}, "done": True,
            }) + "\n")

        terminal = Console(
            file=self.output, force_terminal=True, force_interactive=True,
            legacy_windows=False, _environ={"TERM": "xterm-256color"},
            width=120, height=35,
        )
        with Client(host="http://test", transport=httpx.MockTransport(respond)) as client:
            Agent(client, terminal).run_agent("Execute uma etapa", False)
        names = {t["function"]["name"] for t in requests[0]["tools"]}
        self.assertTrue(TASK_TOOL_NAMES <= names)
        self.assertIn("[in_progress] Etapa visível", requests[1]["messages"][0]["content"])
        output = self.output.getvalue()
        self.assertIn("Etapa visível", output)
        self.assertIn("Concluída", output)
        self.assertIn("\x1b[?25h", output)  # Live restaurou o cursor ao terminar.

    def test_verify_continues_after_failure_and_does_not_invent_api_data(self):
        agent = Agent(Mock(), self.console)
        think, prompt = decide_think("/verify")
        with patch.dict(tools.AVAILABLE_TOOLS, {"today": Mock(side_effect=RuntimeError("falha simulada"))}):
            with patch("agent.agent.stream_model") as stream:
                agent.run_agent(prompt, think)
        stream.assert_not_called()
        statuses = {t.description: t.status for t in agent.tasks.tasks}
        self.assertEqual(statuses["Verificar today"], "failed")
        self.assertEqual(statuses["Verificar calc_data"], "completed")
        self.assertEqual(statuses["Verificar consultar_colaborador"], "blocked")
        self.assertEqual(statuses["Verificar complete_task"], "completed")
        self.assertFalse(agent.tasks.has_pending())
        self.assertIn("Verificação finalizada", self.output.getvalue())
        # Uma pergunta posterior não herda o plano do diagnóstico.
        with patch("agent.agent.stream_model", return_value=("Olá", "", [])):
            agent.run_agent("Olá", False)
        self.assertEqual(agent.tasks.tasks, [])

    def test_verify_default_checks_local_tools_without_llm(self):
        agent = Agent(Mock(), self.console)
        with patch("agent.agent.stream_model") as stream:
            agent.run_agent("/verify", False)
        stream.assert_not_called()
        self.assertEqual(len(agent.tasks.tasks), len(tools.AVAILABLE_TOOLS) + len(TASK_TOOL_NAMES))
        self.assertEqual(sum(t.status == "failed" for t in agent.tasks.tasks), 0)
        self.assertEqual(sum(t.status == "blocked" for t in agent.tasks.tasks), 4)
        self.assertIn("[completed] Verificar today", agent.history[-1]["content"])

    def test_model_error_marks_pending_tasks_and_closes_display(self):
        from ollama import ResponseError
        agent = Agent(Mock(), self.console)
        responses = [
            ("", "", [call("create_tasks", tarefas=["Etapa A"]), call("start_task", task_id=1)]),
            ResponseError("erro de teste", 500),
        ]
        with patch("agent.agent.stream_model", side_effect=responses):
            agent.run_agent("Faça a etapa", False)
        self.assertIsNone(agent.display)
        self.assertEqual(agent.tasks.tasks[0].status, "blocked")
        self.assertIn("erro de teste", agent.tasks.tasks[0].result)


if __name__ == "__main__":
    unittest.main()
