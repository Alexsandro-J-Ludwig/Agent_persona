"""Coordena o turno, o plano de tarefas e a execução de todas as ferramentas.

Ferramentas de negócio ficam em tools/registry.py; ferramentas de plano ficam em
agent/tasks.py. Todas passam pelo mesmo executor e devolvem um resultado por chamada.
"""

import asyncio
import json
from pathlib import Path
from typing import Any

from httpx import RequestError
from ollama import Client, Message, ResponseError
from rich.console import Console

import tools

from .config import MAX_TOOL_ROUNDS, MODEL_CONFIG
from .file_intent import explicit_file_search
from .history import ContextLimitError, create_history, trim_history
from .memory.database import Database
from .router import is_verification_prompt
from .streaming import stream_model
from .tasks import TASK_INSTRUCTIONS, TASK_TOOL_NAMES, TaskDisplay, TaskManager
from .verification import error_message, run_verification

CONTINUATION_PHRASES = (
    "vou tentar",
    "vou buscar",
    "vou pesquisar",
    "vou procurar",
    "vou consultar",
    "vou verificar",
    "vou analisar",
    "agora vou",
    "tentarei buscar",
    "tentarei pesquisar",
    "farei uma busca",
    "vou testar",
    "vou executar",
    "vou ler",
)

CONTINUATION_PROMPT = """
Continue a execução da tarefa anterior. Você afirmou que realizaria outra ação,
mas não chamou a ferramenta necessária. Execute a ação, registre um bloqueio
com o motivo concreto ou apresente o resultado já obtido. Não repita uma promessa.
""".strip()

MAX_IDLE_RESPONSES = 2


# def _exact_json_match(search_result: Any, term: str) -> str | None:
#     if not isinstance(search_result, dict):
#         return None

#     matches = search_result.get("resultados", [])

#     if len(matches) != 1:
#         return None

#     path = Path(matches[0].get("caminho", ""))

#     if (
#         path.suffix.casefold() == ".json"
#         and path.stem.casefold() == Path(term).stem.casefold()
#     ):
#         return str(path)

#     return None


"""Recebe a resposta do modelo e atualiza o terminal progressivamente.

A execução de ferramentas e a manutenção do histórico ficam em agent.py.
Este módulo apenas coleta texto, raciocínio e chamadas de ferramentas.
"""

from contextlib import nullcontext
from typing import Any


class Agent:
    """Uma sessão de conversa. Cada nova mensagem do usuário inicia um plano."""

    def __init__(
        self, client: Client, console: Console, *, display_factory=TaskDisplay
    ):
        self.client = client
        self.console = console
        self.history = create_history()
        self.db = Database()
        self.tasks = TaskManager()
        # A interface é injetada; terminal simples e TUI usam o mesmo agente.
        self.display_factory = display_factory
        self.display: TaskDisplay | None = None

    def lembrar_conversa(
        self,
        content: str,
        role: str | None = None,
        limit: int = 30,
    ) -> str:
        return self.db.lembrar_conversa(
            content=content,
            role=role,
            limit=limit,
        )

    def _execute_agent_tool(self, name: str, args: dict) -> dict:

        if name == "create_tasks":
            self.tasks.create(args["tarefas"])

        elif name == "start_task":
            self.tasks.start(args["task_id"])

        else:
            method = {
                "complete_task": self.tasks.complete,
                "fail_task": self.tasks.fail,
                "block_task": self.tasks.block,
            }[name]

            method(args["task_id"], args.get("resultado"))

        return {"status": "ok", "tarefas": self.tasks.context()}

    async def _execute_tools(self, tool_calls):
        """Executa o lote inteiro, inclusive quando uma chamada anterior falha.

        O try inclui ferramentas internas: task_id inválido não pode encerrar o
        chat. O return fica depois do for; cada tool_call precisa de uma resposta
        no histórico antes da próxima consulta ao modelo.
        """
        executed = []
        batch = []

        async def flush_batch():
            if not batch:
                return

            batch_results = await asyncio.gather(
                *[self._execute_one_tool(call) for call in batch]
            )

            executed.extend(batch_results)
            batch.clear()

        for tool_call in tool_calls:
            name = tool_call.function.name

            if name in TASK_TOOL_NAMES:
                await flush_batch()

                executed.append(await self._execute_one_tool(tool_call))

            else:
                batch.append(tool_call)

        await flush_batch()

        results = []

        for tool_call, result in executed:
            name = tool_call.function.name
            results.append(result)

            # O modelo recebe um resultado por chamada, na ordem original,
            # mesmo quando as ferramentas de negócio executam em paralelo.
            self.history.append(
                {
                    "role": "tool",
                    "tool_name": name,
                    "content": json.dumps(result, ensure_ascii=False, default=str),
                }
            )

        if self.display:
            self.display.refresh()

        return results

    async def _execute_one_tool(self, tool_call):
        name = tool_call.function.name
        args = tool_call.function.arguments

        self.console.print(
            f"\nTool: {name}({args})",
            style="dim",
            markup=False,
        )

        try:
            if name == "lembrar_conversa":
                result = await asyncio.to_thread(self.lembrar_conversa, **args)

            if name in TASK_TOOL_NAMES:
                result = self._execute_agent_tool(name, args)

            else:
                func = tools.AVAILABLE_TOOLS.get(name)

                if func is None:
                    raise ValueError(f"Tool desconhecida: {name}")

                result = await asyncio.to_thread(
                    func,
                    **args,
                )

        except Exception as error:
            result = {"erro": f"{type(error).__name__}: {error}"}

        # Marca a etapa que realmente falhou antes da próxima transição do plano.
        error = error_message(result)
        active = self.tasks.next()

        if (
            error
            and name not in TASK_TOOL_NAMES
            and active
            and active.status == "in_progress"
        ):
            self.tasks.fail(active.id, error)

        if self.display:
            self.display.refresh()

        return tool_call, result

    async def _invoke(self, name: str, args: dict):
        """Chamadas iniciadas pelo código mantêm o mesmo protocolo do modelo."""
        call = Message.ToolCall(function={"name": name, "arguments": args})
        self.history.append({"role": "assistant", "content": "", "tool_calls": [call]})

        results = await self._execute_tools([call])

        return results[0]

    def _messages_for_model(self, reminder: str = ""):
        """Atualiza somente a cópia enviada, sem inventar mensagens do usuário.

        Instruções intermediárias com role=user faziam o corte de histórico perder
        a solicitação original. Qwen também exige system apenas no início.
        """
        messages = [dict(message) for message in self.history]
        state = self.tasks.context() or "Nenhum plano registrado para esta solicitação."
        messages[0]["content"] += (
            "\n\n"
            + TASK_INSTRUCTIONS
            + "\n\nPLANO ATUAL (estado mantido pelo agente):\n"
            + state
            + ("\n\n" + reminder if reminder else "")
        )

        return messages

    def _say(self, text: str, style: str = "yellow"):
        self.console.print(text, style=style, markup=True)
        self.history.append({"role": "assistant", "content": text})

    async def run_agent(self, prompt: str, think_mode: bool, model) -> None:
        from .config import create_client

        # Não carregue tarefas pendentes de um pedido antigo para um novo pedido.
        # O histórico guarda o relatório anterior; um novo plano pode retomá-lo.
        self.tasks.clear()
        self.history.append({"role": "user", "content": prompt})
        self.db.salvar_mensagem(role="user", content=prompt)

        client = create_client()

        with self.display_factory(self.tasks, self.console) as display:
            self.display = display

            try:
                if is_verification_prompt(prompt):
                    report = await run_verification(
                        self.tasks,
                        display,
                        tools.AVAILABLE_TOOLS,
                        tools.tools,
                        self._invoke,
                    )
                    self.history.append({"role": "assistant", "content": report})
                    completed = sum(t.status == "completed" for t in self.tasks.tasks)
                    failed = sum(t.status == "failed" for t in self.tasks.tasks)
                    blocked = sum(t.status == "blocked" for t in self.tasks.tasks)

                    self.console.print(
                        f"Verificação finalizada: {completed} concluídas, {failed} falhas, {blocked} bloqueadas.\n"
                        "Bloqueada significa que a chamada real não foi testada.",
                        markup=False,
                    )

                else:
                    original_prompt = prompt

                    if model == MODEL_CONFIG["agent"]:
                        prompt = f"""
                            Classifique a solicitação abaixo.

                            Use "think" quando exigir raciocínio complexo, planejamento,
                            análise profunda, matemática, arquitetura ou depuração difícil.

                            Use "agent" para tarefas comuns, conversação simples ou execução
                            direta de ferramentas.

                            Responda APENAS com:
                            think
                            ou
                            agent

                            SOLICITAÇÃO:
                            {original_prompt}
                        """

                        response = client.generate(
                            MODEL_CONFIG["agent"]["model"],
                            prompt,
                        )
                        model = response.response.strip().casefold()

                    await self._run_turn(original_prompt, think_mode, model)

            except (
                ResponseError,
                RequestError,
                ConnectionError,
                ContextLimitError,
                OSError,
            ) as error:
                detail = str(error)

                if "no user query found" in detail.casefold():
                    detail = "O servidor não encontrou a pergunta ao montar o contexto. Verifique NUM_CTX e o tamanho do histórico."

                self.tasks.block_pending("Execução interrompida: " + detail)

                self.console.print(
                    f"Falha na execução: {detail}\n"
                    "O chat permanece aberto; o histórico e os resultados das ferramentas foram preservados.",
                    style="red",
                    markup=True,
                )

            except KeyboardInterrupt:
                self.tasks.block_pending("Execução interrompida pelo usuário.")
                raise

            finally:
                display.refresh()
                self.display = None
                self.history = trim_history(self.history)

    async def _run_turn(self, prompt: str, think_mode: bool, model):
        # search_args = explicit_file_search(prompt)

        # if search_args:
        #     search_result = await self._invoke("pesquisar_arquivo", search_args)
        #     search_error = error_message(search_result)

        #     if search_error:
        #         self._say("A busca falhou: " + search_error, "red")
        #         return

        #     if isinstance(search_result, dict) and not search_result.get("resultados"):
        #         if search_result.get("completa") is False:
        #             reminder = (
        #                 "A busca automática inicial pelo arquivo não encontrou resultados, "
        #                 "mas foi PARCIAL. Isso não prova que o arquivo não existe. "
        #                 "Continue a solicitação original. Se necessário, crie um plano e "
        #                 "use as ferramentas disponíveis para explorar diretórios ou pesquisar "
        #                 "outros locais. Não peça ao usuário um diretório antes de tentar "
        #                 "descobri-lo com as ferramentas disponíveis."
        #             )

        #         else:
        #             reminder = (
        #                 "A busca automática inicial não encontrou o arquivo nesse diretório. "
        #                 "Continue a solicitação original e considere outros locais acessíveis."
        #             )

        #         return
        #     path = _exact_json_match(search_result, search_args["termo"])

        #     if path:
        #         await self._invoke("ler_documento", {"caminho": path})

        idle_responses = 0
        reminder = ""

        for _ in range(MAX_TOOL_ROUNDS):
            self.console.print("\n[bold green]IA:[/]", markup=True)

            content, thinking, tool_calls = stream_model(
                self._messages_for_model(reminder),
                think_mode,
                model,
                client=self.client,
                console=self.console,
                display=self.display,
            )

            message = {"role": "assistant", "content": content}

            if thinking:
                message["thinking"] = thinking

            if tool_calls:
                message["tool_calls"] = tool_calls

            self.history.append(message)

            if tool_calls:
                await self._execute_tools(tool_calls)
                idle_responses = 0
                reminder = ""
                continue

            # Texto vazio ou plano pendente não são prova de conclusão. Damos uma
            # chance de recuperação, depois exibimos o bloqueio sem fingir sucesso.
            promised = any(
                phrase in content.casefold() for phrase in CONTINUATION_PHRASES
            )

            if self.tasks.has_pending() or not content.strip() or promised:
                idle_responses += 1

                if idle_responses >= MAX_IDLE_RESPONSES:
                    reason = "O modelo não chamou a ferramenta nem apresentou uma resposta final válida após duas respostas sem progresso."
                    self.tasks.block_pending(reason)
                    self._say(reason, "red")
                    return

                reminder = (
                    CONTINUATION_PROMPT
                    if promised
                    else (
                        "A resposta anterior foi vazia ou ainda há tarefas pendentes. "
                        "Execute a próxima etapa; atualize seu status; se faltar um dado, use block_task e explique."
                    )
                )

                continue

            self.db.salvar_mensagem(role="assistant", content=content)

            return

        reason = "Limite de rodadas atingido antes de uma resposta final."
        self.tasks.block_pending(reason)
        self._say(reason, "red")
