"""Reconhece pedidos explícitos de localizar arquivo para iniciar a busca.

O roteamento é deliberadamente estreito: só usa um nome citado pelo usuário.
Pedidos vagos continuam sob decisão do modelo, sem adivinhar caminhos.
"""

import os
import re

from .config import NETWORK_ROOT

FILE_SUFFIXES = "json|pdf|docx|xlsx|xlsm|csv|txt|md"
NAMED_FILE = re.compile(
    r"\bchamad[oa]\s+[\"'`]?([\w][\w.() -]{0,180})",
    re.IGNORECASE,
)
FILE_WITH_EXTENSION = re.compile(
    rf"\b([\w][\w.()-]*\.(?:{FILE_SUFFIXES}))\b",
    re.IGNORECASE,
)
FILE_WITH_UNDERSCORES = re.compile(r"\b([\w]+(?:_[\w]+)+)\b", re.IGNORECASE)
FILE_CONTEXT = re.compile(r"\b(arquivo|documento|planilha|pdf|json)\b", re.IGNORECASE)
NETWORK_CONTEXT = re.compile(r"\b(rede|network|compartilhament[oa])\b", re.IGNORECASE)


def explicit_file_search(prompt: str) -> dict | None:
    """Retorna argumentos para pesquisar_arquivo quando há nome identificável."""
    if not FILE_CONTEXT.search(prompt):
        return None
    
    match = NAMED_FILE.search(prompt)
    
    if match:
        # A expressão após "chamado" é o nome; ponto final é pontuação.
        term = match.group(1).strip().strip(". ,;:!?\"'`")
        
    else:
        match = FILE_WITH_EXTENSION.search(prompt) or FILE_WITH_UNDERSCORES.search(prompt)
        term = match.group(1) if match else ""
        
    if not term or len(term.split()) > 8:
        return None
    
    args = {"termo": term}
    
    if NETWORK_CONTEXT.search(prompt):
        args["diretorio"] = os.getenv("AGENT_NETWORK_ROOT", NETWORK_ROOT)
        
    return args
