"""
Aqui é apontado as ferramentas no qual o agente de IA pode acessar
"""

from tools.web import read_page, search

from .arquivos import listar_diretorio, pesquisar_arquivo
from .documentos import buscar_documento, ler_documento
from .senior import (
    consultar_colaborador,
    consultar_colaboradores_ativos,
)
from .utilities import (
    calc_data,
    date_previous,
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
    "date_previous": date_previous,
    "calc_data": calc_data,
}
