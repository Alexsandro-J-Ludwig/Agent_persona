"""Escolhe o modo de raciocínio sem fazer uma chamada extra ao modelo.

Adicione novos indicadores em decide_think; os comandos explícitos têm prioridade.
"""

VERIFY_PROMPT = (
    "Teste individualmente todas as ferramentas disponíveis e apresente os resultados."
)


def is_verification_prompt(prompt: str) -> bool:
    return (
        prompt == VERIFY_PROMPT
        or prompt.strip().casefold().split(" ", 1)[0] == "/verify"
    )


def decide_think(prompt: str) -> tuple[bool, dict, str]:
    """Seleciona inicialmente o modelo.

    Comandos explícitos bloqueiam a seleção.
    Heurísticas resolvem casos óbvios.
    Casos comuns podem ser reavaliados pelo Agent.
    """

    from agent.config import MODEL_CONFIG

    prompt = prompt.strip()
    lower = prompt.casefold()

    # Comandos explícitos
    if lower == "/think":
        return True, MODEL_CONFIG["think"], ""

    if lower.startswith("/think "):
        return True, MODEL_CONFIG["think"], prompt[7:].strip()

    if lower == "/fast":
        return False, MODEL_CONFIG["agent"], ""

    if lower.startswith("/fast "):
        return False, MODEL_CONFIG["agent"], prompt[6:].strip()

    if lower == "/verify" or lower.startswith("/verify "):
        return False, MODEL_CONFIG["agent"], VERIFY_PROMPT

    indicadores_complexos = (
        "analise",
        "analisa",
        "explique detalhadamente",
        "resolva",
        "calcule",
        "demonstre",
        "prove",
        "debug",
        "corrija",
        "otimize",
        "refatore",
        "algoritmo",
        "arquitetura",
        "código",
        "codigo",
        "sql",
        "matemática",
        "matematica",
        "derivada",
        "integral",
        "limite",
        "matriz",
        "vetor",
    )

    # Casos obviamente complexos nem precisam do classificador.
    if len(prompt) > 350:
        return True, MODEL_CONFIG["think"], prompt

    if any(indicador in lower for indicador in indicadores_complexos):
        return True, MODEL_CONFIG["think"], prompt

    # Caso aparentemente simples.
    # Agent ainda pode avaliar se precisa escalar.
    return False, MODEL_CONFIG["agent"], prompt
