"""Comunicação com Ollama e exibição incremental da resposta no terminal.

Este módulo coleta conteúdo, raciocínio e ferramentas; não altera o histórico
e não executa ferramentas. Isso mantém a apresentação separada da orquestração.
"""

from typing import Any

from ollama import Client
from rich.console import Console
from rich.live import Live
from rich.markdown import Markdown

from .status_messages import processing_message

import tools
from .config import KEEP_ALIVE, MODEL, NUM_CTX
from .history import prepare_messages
from .tasks import TASK_TOOLS, TaskDisplay

def stream_model(
    messages: list[dict[str, Any]],
    think_mode: bool,
    *,
    client: Client,
    console: Console,
    display: TaskDisplay | None = None,
):
    """
    Executa uma rodada do modelo com streaming.

    Retorna:
        content
        thinking
        tool_calls
    """

    # Ferramentas de plano pertencem à sessão; precisam ser anunciadas junto
    # das ferramentas de negócio para que o modelo consiga chamá-las.
    definitions = tools.tools + TASK_TOOLS
    stream = client.chat(
        model=MODEL,
        messages=prepare_messages(messages, definitions),
        think=think_mode,
        tools=definitions,
        keep_alive=KEEP_ALIVE,
        stream=True,
        options={"num_ctx": NUM_CTX},
    )

    content = ""
    thinking = ""
    tool_calls = []

    if display is not None:
        # O Agent já abriu um painel Live para o turno inteiro. Reutilize-o,
        # em vez de iniciar outro Live/status que deslocaria a lista de tarefas.
        display.status(processing_message())
        try:
            for chunk in stream:
                thinking += chunk.message.thinking or ""
                tool_calls.extend(chunk.message.tool_calls or [])
                if chunk.message.content:
                    content += chunk.message.content
                    display.response(content)
        finally:
            if hasattr(stream, "close"):
                stream.close()
            display.finish_response(content)
        return content, thinking, tool_calls

    # --------------------------------------------------------
    # Espera até aparecer conteúdo.
    #
    # console.status possui animação própria em background,
    # então continua animando mesmo enquanto esperamos
    # o primeiro chunk da rede/modelo.
    # --------------------------------------------------------

    status_text = (
        f"[bold red]{processing_message()}[/bold red]"
        if think_mode
        else f"[bold yellow]{processing_message()}[/bold yellow]"
    )

    first_content = False

    with console.status(
        status_text,
        spinner="dots",
        spinner_style="red" if think_mode else "yellow",
    ):
        for chunk in stream:

            if chunk.message.thinking:
                thinking += chunk.message.thinking

            if chunk.message.tool_calls:
                tool_calls.extend(chunk.message.tool_calls)

            if chunk.message.content:
                content += chunk.message.content
                first_content = True
                break

    # --------------------------------------------------------
    # Se começou a resposta visível, renderiza Markdown
    # em streaming.
    # --------------------------------------------------------

    if first_content:
        with Live(
            Markdown(content),
            console=console,
            refresh_per_second=15,
        ) as live:

            for chunk in stream:

                if chunk.message.thinking:
                    thinking += chunk.message.thinking

                if chunk.message.tool_calls:
                    tool_calls.extend(
                        chunk.message.tool_calls
                    )

                if chunk.message.content:
                    content += chunk.message.content
                    live.update(Markdown(content))

    return content, thinking, tool_calls
