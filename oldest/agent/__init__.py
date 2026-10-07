"""API pública do pacote de conversação.

config: parâmetros e cliente; history: contexto; router: modo de raciocínio;
streaming: rede e renderização; agent: coordenação e execução de ferramentas.
"""

from .agent import Agent
from .router import decide_think

__all__ = ["Agent", "decide_think"]
