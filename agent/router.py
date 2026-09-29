"""Escolhe o modo de raciocínio sem fazer uma chamada extra ao modelo.

Adicione novos indicadores em decide_think; os comandos explícitos têm prioridade.
"""

VERIFY_PROMPT = "Teste individualmente todas as ferramentas disponíveis e apresente os resultados."


def is_verification_prompt(prompt: str) -> bool:
    return prompt == VERIFY_PROMPT or prompt.strip().casefold().split(" ", 1)[0] == "/verify"


def decide_think(prompt: str) -> tuple[bool, str]:
    """
    Decide rapidamente se deve usar thinking.

    /think força thinking.
    /fast força resposta sem thinking.

    Sem comando explícito, usa heurística local.
    Isso evita fazer uma segunda chamada ao LLM apenas
    para decidir se ele deve pensar.
    """

    prompt = prompt.strip()
    lower = prompt.lower()

    if lower == "/think":
        return True, ""

    if lower.startswith("/think "):
        return True, prompt[7:].strip()

    if lower == "/fast":
        return False, ""

    if lower.startswith("/fast "):
        return False, prompt[6:].strip()

    if lower == "/verify" or lower.startswith("/verify "):
        # O Agent reconhece esse comando e executa verificações controladas;
        # uma instrução em linguagem natural não garantia a cobertura da lista.
        return False, VERIFY_PROMPT
        

    texto = prompt.casefold()

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

    if len(prompt) > 350:
        return True, prompt

    if any(indicador in texto for indicador in indicadores_complexos):
        return True, prompt

    return False, prompt
