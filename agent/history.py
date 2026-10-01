"""Criação e retenção do contexto de cada sessão de conversa."""

from typing import Any
import json

from .config import MAX_HISTORY_MESSAGES, MAX_INPUT_BYTES, SYSTEM_PROMPT
from .memory.compressor import HistoryCompressor


class ContextLimitError(ValueError):
    """O turno atual não cabe no orçamento local sem perder informações."""


def prepare_messages(history, tool_definitions, max_bytes=MAX_INPUT_BYTES):
    """Prepara uma cópia para envio, removendo somente turnos antigos inteiros.

    Bytes UTF-8 são uma estimativa conservadora, não uma contagem de tokens.
    Incluímos schemas e raciocínio no orçamento, com margem para o template e
    a resposta. Nunca cortamos silenciosamente um documento ou a pergunta atual.
    """
    messages = [dict(message) for message in history]
    trim_history(messages)
    if not any(m.get("role") == "user" for m in messages):
        raise ValueError("O histórico precisa conter a pergunta do usuário.")

    while True:
        payload = json.dumps(
            {"messages": messages, "tools": tool_definitions},
            ensure_ascii=False,
            default=lambda value: value.model_dump(exclude_none=True),
        )

        if len(payload.encode("utf-8")) <= max_bytes:
            return messages

        user_indexes = [i for i, m in enumerate(messages) if m.get("role") == "user"]

        if len(user_indexes) < 2:
            raise ContextLimitError(
                "A pergunta e os resultados deste turno excedem o limite de contexto. "
                "Solicite um trecho menor do documento ou ajuste NUM_CTX e MAX_INPUT_BYTES "
                "em agent/config.py conforme a capacidade do servidor."
            )

        del messages[user_indexes[0] : user_indexes[1]]


def create_history() -> list[dict[str, Any]]:
    """Cria uma lista independente para cada sessão."""
    return [{"role": "system", "content": SYSTEM_PROMPT}]


def trim_history(
    history: list[dict[str, Any]],
    max_messages: int = MAX_HISTORY_MESSAGES,
) -> list[dict[str, Any]]:
    """Remove turnos antigos inteiros preservando a instrução inicial.

    Um último turno maior que o limite é mantido inteiro para não separar
    chamadas de ferramentas de seus resultados. Execute após concluir o turno.
    """

    if len(history) <= max_messages:
        return history

    user_indexes = [
        index for index, message in enumerate(history) if message.get("role") == "user"
    ]

    if len(user_indexes) < 2:
        return history

    compressor = HistoryCompressor()

    while len(history) > max_messages:
        user_indexes = [
            index
            for index, message in enumerate(history)
            if message.get("role") == "user"
        ]

        if len(user_indexes) < 2:
            break

        inicio = user_indexes[0]
        fim = user_indexes[1]

        turno_antigo = history[inicio:fim]

        resumo = compressor.comprimir(turno_antigo)

        # Descarta também instruções de continuação pertencentes ao turno antigo.
        del history[user_indexes[0] : user_indexes[1]]

        history.insert(
            1, {"role": "system", "content": f"Resuo do historico anterior:\n{resumo}"}
        )

    return history
