"""Conversa rolável com entrada fixa. O agente comunica-se por uma fila.

Somente a thread do Textual manipula widgets. Rede, banco e ferramentas ficam
no worker, para que a interface continue respondendo durante a geração.
"""

import asyncio
from io import StringIO
from queue import Empty, SimpleQueue

from rich.console import Console
from rich.markdown import Markdown
from rich.text import Text
from textual import work
from textual.app import App, ComposeResult
from textual.containers import VerticalScroll
from textual.css.query import NoMatches
from textual.widgets import Input, Static

from .agent import Agent
from .config import create_client
from .router import decide_think


class TerminalConsole(Console):
    """Encaminha mensagens do agente sem imprimir sobre o rodapé."""

    def __init__(self, events: SimpleQueue):
        super().__init__(
            file=StringIO(), width=120, force_terminal=True, color_system="truecolor"
        )
        self.events = events

    def print(self, *objects, **kwargs):
        with self.capture() as capture:
            super().print(*objects, **kwargs)

        self.events.put(("print", Text.from_ansi(capture.get())))


class TerminalDisplay:
    """Contrato de TaskDisplay sem criar um segundo mecanismo de desenho."""

    def __init__(self, manager, console: TerminalConsole):
        self.manager = manager
        self.events = console.events
        self.last_state = None

    def __enter__(self):
        self.events.put(("turn", None))
        return self

    def status(self, message: str):
        self.events.put(("status", message))

    def append_response(self, content: str):
        self.events.put(("delta", content))

    def response(self, content: str):
        self.events.put(("replace", content))

    def finish_response(self, content: str):
        self.events.put(("finish", content))

    def refresh(self):
        state = self.manager.context()

        if state != self.last_state:
            self.last_state = state
            panel = self.manager.render() if state else None
            self.events.put(("tasks", panel))

    def __exit__(self, *exc):
        self.refresh()


class AgentTerminal(App):
    """Atualiza a resposta ativa e preserva as mensagens concluídas."""

    CSS = """
    Screen { layout: vertical; }
    #conversation { height: 1fr; padding: 0 1; }
    #conversation Static { height: auto; margin-bottom: 1; }
    #prompt { dock: bottom; border: solid cyan; }
    """

    BINDINGS = [  # noqa: RUF012
        ("pageup", "history_up", "Subir conversa"),
        ("pagedown", "history_down", "Descer conversa"),
        ("ctrl+end", "history_end", "Última mensagem"),
        ("ctrl+q", "request_exit", "Sair"),
    ]

    def __init__(self, *, agent_factory=Agent, client_factory=create_client):
        super().__init__()
        self.events = SimpleQueue()
        self.agent_factory = agent_factory
        self.client_factory = client_factory
        self.agent = None
        self.busy = False
        self.exit_after_turn = False
        self.current_response = None
        self.current_text = Text("")
        self.current_tasks = None

    def compose(self) -> ComposeResult:
        with VerticalScroll(id="conversation"):
            yield Static(
                "[dim][red]/help:[/] para ver os comandos[/]\n\n"
                "Mouse ou Page Up/Down: rolar. Ctrl+End: voltar ao final.\n"
                "[dim][cyan]As respostas podem conter erros; confirme informações importantes.[/][/]",
                markup=True,
            )

        prompt = Input(placeholder="Escreva sua mensagem", id="prompt")
        prompt.border_title = "User"
        yield prompt

    def on_mount(self):
        self.query_one(Input).focus()

        # Um único relógio agrupa tokens e atualiza a interface. Não depende
        # da chegada do próximo token para exibir o trecho já recebido.
        self.refresh_timer = self.set_interval(0.1, self.drain_events)

    def on_unmount(self):
        self.refresh_timer.stop()

    async def on_input_submitted(self, event: Input.Submitted):
        prompt = event.value.strip()

        if prompt.casefold() in {"/sair", "/exit"}:
            self.action_request_exit()
            return

        if prompt.casefold() in {"/help", "/ajuda"}:
            conversation = self.query_one("#conversation", VerticalScroll)

            help_text = Text()
            help_text.append("/think", style="bold red")
            help_text.append("  Com raciocínio\n")

            help_text.append("/fast", style="bold red")
            help_text.append("  Sem raciocínio\n")

            help_text.append("/verify", style="bold red")
            help_text.append("  Validar ferramentas\n")

            help_text.append("/sair", style="bold red")
            help_text.append("  Encerrar")

            await conversation.mount(Static(help_text))
            conversation.scroll_end(animate=False)
            return

        if not prompt:
            return

        if self.busy:
            self.notify("Aguarde a resposta atual. Seu texto continua no campo.")
            return

        event.input.value = ""
        event.input.placeholder = "User: respondendo… pode preparar a próxima pergunta"
        self.busy = True

        conversation = self.query_one("#conversation", VerticalScroll)
        await conversation.mount(Static(Text("User: " + prompt, style="cyan")))
        conversation.scroll_end(animate=False)
        self.run_turn(prompt)

    @work(thread=True, exclusive=True)
    def run_turn(self, prompt: str):
        """O cliente síncrono não deve rodar na thread da interface."""
        try:
            if self.agent is None:
                self.agent = self.agent_factory(
                    self.client_factory(),
                    TerminalConsole(self.events),
                    display_factory=TerminalDisplay,
                )

            think_mode, clean_prompt = decide_think(prompt)
            asyncio.run(self.agent.run_agent(clean_prompt, think_mode))

        except Exception as error:
            self.events.put(("print", Text(f"Falha: {error}", style="red")))

        finally:
            self.events.put(("done", None))

    async def drain_events(self):
        try:
            conversation = self.query_one("#conversation", VerticalScroll)

        except NoMatches:
            # Um último tick pode coincidir com a desmontagem da tela.
            return

        follow_end = conversation.is_vertical_scroll_end
        changed = False
        text_changed = False

        # O limite deixa tempo para teclas e mouse mesmo com muitos eventos.
        for _ in range(1000):
            try:
                kind, value = self.events.get_nowait()

            except Empty:
                break

            changed = True

            if kind == "turn":
                self.current_tasks = None

            elif kind == "print":
                await conversation.mount(Static(value))

            elif kind == "status":
                self.current_text = Text("")
                self.current_response = Static(Text(value, style="dim"))
                await conversation.mount(self.current_response)

            elif kind in {"delta", "replace"}:
                if self.current_response is None:
                    self.current_response = Static()
                    await conversation.mount(self.current_response)

                if kind == "replace":
                    self.current_text = Text(value)

                else:
                    self.current_text.append(value)

                text_changed = True

            elif kind == "finish":
                if self.current_response is not None:
                    if value:
                        # Formata uma vez e substitui o widget ativo. Não cria
                        # outra cópia da resposta nem apaga a conversa anterior.
                        self.current_response.update(Markdown(value))

                    else:
                        await self.current_response.remove()

                self.current_response = None
                self.current_text = Text("")
                text_changed = False

            elif kind == "tasks" and value is not None:
                if self.current_tasks is None:
                    self.current_tasks = Static(value)
                    await conversation.mount(self.current_tasks)

                else:
                    self.current_tasks.update(value)

            elif kind == "done":
                self.busy = False
                self.query_one(Input).placeholder = "User: escreva sua mensagem"

                if self.exit_after_turn:
                    self.exit()

        if text_changed and self.current_response is not None:
            self.current_response.update(self.current_text.copy())

        # Quem está lendo mensagens antigas não é arrastado para o final.
        if changed and follow_end:
            self.call_after_refresh(conversation.scroll_end, animate=False)

    def action_history_up(self):
        self.query_one("#conversation", VerticalScroll).scroll_page_up(animate=False)

    def action_history_down(self):
        self.query_one("#conversation", VerticalScroll).scroll_page_down(animate=False)

    def action_history_end(self):
        self.query_one("#conversation", VerticalScroll).scroll_end(animate=False)

    def action_request_exit(self):
        if self.busy:
            # A saída normal aguarda ferramentas em andamento: threads não
            # podem ser interrompidas com segurança no meio de uma operação.
            self.exit_after_turn = True
            self.notify("Vou encerrar assim que a execução atual terminar.")

        else:
            self.exit()
