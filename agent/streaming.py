"""Recebe a resposta do modelo e atualiza o terminal progressivamente.

A execução de ferramentas e a manutenção do histórico ficam em agent.py.
Este módulo apenas coleta texto, raciocínio e chamadas de ferramentas.
"""

from contextlib import nullcontext
from typing import Any

from ollama import Client
from rich.console import Console

import tools

from .config import MODEL_CONFIG
from .history import prepare_messages
from .status_messages import processing_message
from .tasks import TASK_TOOLS, TaskDisplay, TaskManager


def stream_model(
    messages: list[dict[str, Any]],
    think_mode: bool,
    model: str,
    *,
    client: Client,
    console: Console,
    display: TaskDisplay | None = None,
):
    """Executa uma rodada do modelo com streaming.

    Retorna:
        content: texto da resposta.
        thinking: raciocínio recebido.
        tool_calls: ferramentas solicitadas pelo modelo.

    Erros continuam sendo tratados pelo agente que chamou esta função.
    """

    definitions = tools.tools + TASK_TOOLS
    prepared = prepare_messages(messages, definitions)

    content_parts: list[str] = []
    thinking_parts: list[str] = []
    tool_calls = []
    stream = None

    # O agente já mantém um painel durante o turno.
    # nullcontext permite usá-lo sem abri-lo ou fechá-lo novamente.
    # Chamadas independentes recebem um painel temporário.
    display_context = (
        nullcontext(display)
        if display is not None
        else TaskDisplay(TaskManager(), console)
    )

    with display_context as panel:
        panel.status(processing_message())

        try:
            stream = client.chat(
                model=MODEL_CONFIG[model]["model"],
                messages=prepared,
                think=think_mode,
                tools=definitions,
                keep_alive=MODEL_CONFIG[model]["keep_alive"],
                stream=True,
                options={"num_ctx": MODEL_CONFIG[model]["num_ctx"]},
            )

            # Um único laço consome todos os trechos da resposta.
            for chunk in stream:
                message = chunk.message

                if message.thinking:
                    thinking_parts.append(message.thinking)

                if message.tool_calls:
                    tool_calls.extend(message.tool_calls)

                if message.content:
                    content_parts.append(message.content)

                    # Entrega apenas o trecho novo. A interface controla o ritmo
                    # de desenho; nenhum token fica esperando o próximo chegar.
                    # Uma futura fila de voz pode receber este mesmo trecho.
                    panel.append_response(message.content)

        finally:
            # Libera a conexão mesmo se ocorrer erro ou Ctrl+C.
            # O painel também recebe o último trecho, que pode ter chegado
            # antes de completar o intervalo de atualização.
            try:
                if stream is not None:
                    close = getattr(stream, "close", None)
                    if callable(close):
                        close()
            finally:
                panel.finish_response("".join(content_parts))

        return (
            "".join(content_parts),
            "".join(thinking_parts),
            tool_calls,
        )
