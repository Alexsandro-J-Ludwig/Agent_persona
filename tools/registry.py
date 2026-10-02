"""
Aqui é apontado as ferramentas no qual o agente de IA pode acessar
"""

from tools.native.web import read_page, search

from .extern.senior import (
    consultar_colaborador,
    consultar_colaboradores_ativos,
)
from .native.arquivos import listar_diretorio, pesquisar_arquivo
from .native.documentos import buscar_documento, ler_documento
from .native.utilities import (
    today,
)

AVAILABLE_TOOLS = {
    "consultar_colaborador": consultar_colaborador,
    "consultar_colaboradores_ativos": consultar_colaboradores_ativos,
    "pesquisar_arquivo": pesquisar_arquivo,
    "ler_documento": ler_documento,
    "buscar_documento": buscar_documento,
    "listar_diretorio": listar_diretorio,
    "search": search,
    "read_page": read_page,
    "today": today,
}
