"""Regressões do contexto após ferramentas, usando o SDK real sem rede."""

from copy import deepcopy
from io import StringIO
import json
import unittest
from unittest.mock import Mock, patch

import httpx
from ollama import Client, Message, ResponseError
from rich.console import Console

from agent.agent import Agent, CONTINUATION_PROMPT
from agent.file_intent import explicit_file_search
from agent.config import NUM_CTX
from agent.history import ContextLimitError, prepare_messages
from agent.streaming import stream_model


class AgentContextTests(unittest.TestCase):
    def setUp(self):
        self.console = Console(file=StringIO())
        self.call = Message.ToolCall(function={"name": "ler_documento", "arguments": {"caminho": "teste.pdf"}})
        self.messages = [
            {"role": "system", "content": "Instruções"},
            {"role": "user", "content": "Leia o documento"},
            {"role": "assistant", "content": "", "thinking": "Raciocínio", "tool_calls": [self.call]},
            {"role": "tool", "tool_name": "ler_documento", "content": "Conteúdo"},
        ]

    def test_sdk_preserva_usuario_e_num_ctx_no_payload(self):
        captured = []

        def respond(request):
            captured.append(json.loads(request.content))
            return httpx.Response(200, text=json.dumps({
                "model": "teste", "message": {"role": "assistant", "content": "OK"}, "done": True,
            }) + "\n")

        with Client(host="http://test", transport=httpx.MockTransport(respond)) as client:
            content, _, _ = stream_model(self.messages, False, client=client, console=self.console)
        self.assertEqual(content, "OK")
        self.assertEqual(captured[0]["options"]["num_ctx"], NUM_CTX)
        self.assertEqual([m["role"] for m in captured[0]["messages"]], ["system", "user", "assistant", "tool"])
        self.assertEqual(captured[0]["messages"][-1]["tool_name"], "ler_documento")

    def test_remove_so_turnos_antigos(self):
        history = self.messages[:1] + [
            {"role": "user", "content": "Antigo"},
            {"role": "assistant", "content": "x" * 5000},
        ] + self.messages[1:]
        original = deepcopy(history)
        self.assertEqual(prepare_messages(history, [], max_bytes=1000), self.messages)
        self.assertEqual(history, original)

    def test_turno_grande_nao_e_cortado_silenciosamente(self):
        with self.assertRaises(ContextLimitError):
            prepare_messages(self.messages, [], max_bytes=10)
        with self.assertRaises(ValueError):
            prepare_messages([{"role": "tool", "content": "órfão"}], [])

    def test_erro_durante_stream_nao_encerra_agente_nem_repete_tool(self):
        captured = []

        def respond(request):
            captured.append(json.loads(request.content))
            if len(captured) == 1:
                return httpx.Response(200, text=json.dumps({
                    "model": "teste", "message": {"role": "assistant", "content": "", "tool_calls": [self.call.model_dump()]}, "done": True,
                }) + "\n")
            return httpx.Response(500, text="No user query found in messages.")

        reader = Mock(return_value="Conteúdo do documento")
        with Client(host="http://test", transport=httpx.MockTransport(respond)) as client:
            agent = Agent(client, self.console)
            with patch.dict("tools.AVAILABLE_TOOLS", {"ler_documento": reader}):
                agent.run_agent("Leia o documento", False)
        reader.assert_called_once()
        self.assertEqual(agent.history[-1]["role"], "tool")
        self.assertIn("O chat permanece aberto", self.console.file.getvalue())
        self.assertEqual(len(captured), 2)

    def test_continuacao_nao_insere_system_no_meio(self):
        agent = Agent(Mock(), self.console)
        replies = [("", "", [self.call]), ("Vou verificar", "", []), ("Concluído", "", [])]
        with patch("agent.agent.stream_model", side_effect=replies) as stream, patch.dict("tools.AVAILABLE_TOOLS", {"ler_documento": lambda **kwargs: "Conteúdo"}):
            agent.run_agent("Leia o documento", False)
        self.assertEqual([i for i, m in enumerate(agent.history) if m["role"] == "system"], [0])
        self.assertIn(CONTINUATION_PROMPT, stream.call_args_list[-1].args[0][0]["content"])
        self.assertEqual(sum(m["role"] == "user" for m in agent.history), 1)

    def test_nome_explicito_na_rede_dispara_busca_e_leitura(self):
        prompt = (
            'Tem um arquivo na rede que quero que você me informe sobre ele, '
            'é um arquivo json chamado Adservi_postman_collection'
        )
        self.assertEqual(
            explicit_file_search(prompt),
            {'termo': 'Adservi_postman_collection', 'diretorio': 'Z:/'},
        )
        self.assertIsNone(explicit_file_search('O que é JSON?'))
        search = Mock(return_value={
            'resultados': [{
                'nome': 'Adservi_postman_collection.json',
                'caminho': 'Z:/Adservi_postman_collection.json',
            }],
            'completa': True,
        })
        read = Mock(return_value={
            'conteudo': '{"info": {"name": "Adservi"}}', 'tem_mais': False,
        })
        agent = Agent(Mock(), self.console)
        with patch('agent.agent.stream_model', return_value=('É uma coleção Postman.', '', [])):
            with patch.dict('tools.AVAILABLE_TOOLS', {
                'pesquisar_arquivo': search, 'ler_documento': read,
            }):
                agent.run_agent(prompt, False)
        search.assert_called_once_with(
            termo='Adservi_postman_collection', diretorio='Z:/'
        )
        read.assert_called_once()
        self.assertEqual(
            read.call_args.kwargs['caminho'].replace('\\', '/'),
            'Z:/Adservi_postman_collection.json',
        )
        self.assertEqual(
            [m['role'] for m in agent.history],
            ['system', 'user', 'assistant', 'tool', 'assistant', 'tool', 'assistant'],
        )

    def test_promessa_sem_ferramenta_nao_vira_resposta_final(self):
        agent = Agent(Mock(), self.console)
        with patch('agent.agent.stream_model', side_effect=[
            ('Vou pesquisar o arquivo.', '', []),
            ('Vou buscar agora.', '', []),
        ]) as stream:
            agent.run_agent('Pesquise isso', False)
        self.assertEqual(stream.call_count, 2)
        self.assertIn('não chamou a ferramenta', self.console.file.getvalue())

    def test_busca_parcial_na_rede_pede_subpasta_sem_alegar_ausencia(self):
        agent = Agent(Mock(), self.console)
        prompt = 'Procure o arquivo json chamado Adservi_postman_collection na rede'
        with patch.dict('tools.AVAILABLE_TOOLS', {
            'pesquisar_arquivo': lambda **kwargs: {
                'resultados': [], 'completa': False,
                'arquivos_examinados': 1731,
            },
        }):
            with patch('agent.agent.stream_model') as stream:
                agent.run_agent(prompt, False)
        stream.assert_not_called()
        self.assertIn('busca em Z:/ foi parcial', self.console.file.getvalue())
        self.assertIn('subpasta', self.console.file.getvalue())


if __name__ == "__main__":
    unittest.main()
