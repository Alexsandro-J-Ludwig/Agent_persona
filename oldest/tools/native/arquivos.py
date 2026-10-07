"""Pesquisa arquivos por nome no Windows.

Estratégia:
1. Windows Search (índice) para buscas rápidas em locais indexados.
2. where.exe /r como fallback para diretórios não indexados, inclusive unidades
   de rede quando acessíveis.
3. Python apenas classifica os candidatos encontrados; não percorre toda a
   árvore com os.walk().
"""

from __future__ import annotations

import os
import re
import subprocess
import unicodedata
from difflib import SequenceMatcher
from pathlib import Path

MAX_RESULTADOS = 50
TIMEOUT_INDICE = 8
TIMEOUT_FALLBACK = 30


# ---------------------------------------------------------------------------
# Normalização
# ---------------------------------------------------------------------------


def _normalizar(texto: str) -> str:
    texto = unicodedata.normalize("NFKD", texto.casefold())

    return "".join(
        caractere for caractere in texto if not unicodedata.combining(caractere)
    )


def _palavras(texto: str) -> list[str]:
    return re.findall(r"[^\W_]+", _normalizar(texto))


# ---------------------------------------------------------------------------
# Pontuação
# ---------------------------------------------------------------------------


def _pontuacao(termo: str, nome: str) -> float:
    """
    Mede a semelhança sem exigir que absolutamente todas as palavras coincidam.

    Prioridades:
    - nome exato;
    - termo contido no nome;
    - palavras exatas;
    - palavras semelhantes.
    """

    termo_normalizado = _normalizar(termo)
    nome_normalizado = _normalizar(Path(nome).stem)

    if termo_normalizado == nome_normalizado:
        return 1.0

    if termo_normalizado in nome_normalizado:
        return 0.98

    termos = _palavras(termo)
    palavras_nome = _palavras(nome_normalizado)

    if not termos or not palavras_nome:
        return 0.0

    notas: list[float] = []

    for palavra in termos:
        melhor = 0.0

        for candidata in palavras_nome:
            if palavra == candidata:
                nota = 1.0

            elif palavra in candidata or candidata in palavra:
                nota = 0.90

            elif len(palavra) >= 4 and len(candidata) >= 4:
                nota = SequenceMatcher(
                    None,
                    palavra,
                    candidata,
                ).ratio()

            else:
                nota = 0.0

            melhor = max(melhor, nota)

        notas.append(melhor)

    # Não deixa uma palavra irrelevante destruir todo o resultado.
    boas = [nota for nota in notas if nota >= 0.55]

    if not boas:
        return 0.0

    cobertura = len(boas) / len(termos)
    qualidade = sum(boas) / len(boas)

    # Cobertura tem mais peso que fuzzy matching.
    nota_final = cobertura * 0.65 + qualidade * 0.35

    # Evita candidatos muito vagos.
    if cobertura < 0.5:
        return 0.0

    return nota_final


# ---------------------------------------------------------------------------
# Windows Search
# ---------------------------------------------------------------------------


def _escapar_sql(texto: str) -> str:
    return texto.replace("'", "''")


def _buscar_indice(
    termo: str,
    diretorio: Path,
) -> tuple[list[str], list[str]]:
    """
    Consulta o índice do Windows Search usando PowerShell + OLE DB.

    Não percorre os arquivos manualmente.
    """

    palavras = _palavras(termo)

    if not palavras:
        return [], []

    # O Windows Search trabalha com URLs file:.
    scope = diretorio.resolve().as_uri()

    condicoes = " AND ".join(
        f"System.FileName LIKE '%{_escapar_sql(palavra)}%'" for palavra in palavras
    )

    query = (
        "SELECT TOP 200 System.ItemPathDisplay "
        "FROM SYSTEMINDEX "
        f"WHERE SCOPE = '{_escapar_sql(scope)}' "
        f"AND {condicoes}"
    )

    powershell = r"""
$ErrorActionPreference = "Stop"

$connection = New-Object System.Data.OleDb.OleDbConnection
$connection.ConnectionString = "Provider=Search.CollatorDSO;Extended Properties='Application=Windows';"
$connection.Open()

try {
    $command = $connection.CreateCommand()
    $command.CommandText = $env:INCUBE_SEARCH_QUERY

    $reader = $command.ExecuteReader()

    while ($reader.Read()) {
        $path = $reader.GetValue(0)

        if ($null -ne $path) {
            Write-Output $path
        }
    }
}
finally {
    if ($reader) {
        $reader.Close()
    }

    $connection.Close()
}
"""

    ambiente = os.environ.copy()
    ambiente["INCUBE_SEARCH_QUERY"] = query

    try:
        processo = subprocess.run(
            [
                "powershell.exe",
                "-NoProfile",
                "-NonInteractive",
                "-Command",
                powershell,
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=TIMEOUT_INDICE,
            env=ambiente,
            creationflags=getattr(
                subprocess,
                "CREATE_NO_WINDOW",
                0,
            ),
        )

    except subprocess.TimeoutExpired:
        return [], ["Windows Search excedeu o tempo limite."]

    except OSError as erro:
        return [], [f"Windows Search indisponível: {erro}"]

    if processo.returncode != 0:
        erro = processo.stderr.strip()

        return [], [
            "Windows Search não pôde pesquisar esse local"
            + (f": {erro}" if erro else ".")
        ]

    caminhos = [
        linha.strip() for linha in processo.stdout.splitlines() if linha.strip()
    ]

    return caminhos, []


# ---------------------------------------------------------------------------
# Fallback nativo
# ---------------------------------------------------------------------------


def _buscar_where(
    termo: str,
    diretorio: Path,
) -> tuple[list[str], list[str], bool]:
    """
    Usa where.exe para enumerar arquivos nativamente.

    É particularmente útil quando o Windows Search não indexa o diretório.
    """

    palavras = _palavras(termo)

    if not palavras:
        return [], [], True

    # Usa a palavra mais significativa para reduzir a quantidade de resultados.
    palavra_base = max(palavras, key=len)

    padrao = f"*{palavra_base}*"

    try:
        processo = subprocess.run(
            [
                "where.exe",
                "/r",
                str(diretorio),
                padrao,
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=TIMEOUT_FALLBACK,
            creationflags=getattr(
                subprocess,
                "CREATE_NO_WINDOW",
                0,
            ),
        )

    except subprocess.TimeoutExpired as erro:
        # subprocess pode trazer parte do stdout mesmo no timeout.
        stdout = erro.stdout or ""

        if isinstance(stdout, bytes):
            stdout = stdout.decode("utf-8", errors="replace")

        caminhos = [linha.strip() for linha in stdout.splitlines() if linha.strip()]

        return (
            caminhos,
            [
                "A busca atingiu o limite de tempo. "
                "Os resultados retornados são parciais."
            ],
            False,
        )

    except OSError as erro:
        return (
            [],
            [f"Não foi possível executar where.exe: {erro}"],
            False,
        )

    # where retorna 1 quando não encontra nada.
    if processo.returncode not in {0, 1}:
        erro = processo.stderr.strip()

        return (
            [],
            [f"where.exe falhou: {erro}"],
            False,
        )

    caminhos = [
        linha.strip() for linha in processo.stdout.splitlines() if linha.strip()
    ]

    return caminhos, [], True


# ---------------------------------------------------------------------------
# Pesquisa pública
# ---------------------------------------------------------------------------


def pesquisar_arquivo(
    termo: str,
    diretorio: str | None = None,
):
    """
    Pesquisa arquivos pelo nome.

    Usa primeiro o índice do Windows. Caso não encontre candidatos suficientes,
    utiliza where.exe como fallback.

    Similaridade representa apenas a qualidade do nome encontrado.
    O conteúdo do documento deve ser verificado separadamente.
    """

    raiz = Path(diretorio).expanduser() if diretorio else Path.home()

    if not raiz.exists():
        return {
            "resultados": [],
            "diretorio": str(raiz),
            "completa": False,
            "metodo": None,
            "avisos": [f"Diretório inexistente ou inacessível: {raiz}"],
        }

    if not raiz.is_dir():
        return {
            "resultados": [],
            "diretorio": str(raiz),
            "completa": False,
            "metodo": None,
            "avisos": [f"O caminho informado não é um diretório: {raiz}"],
        }

    if not _palavras(termo):
        return {
            "resultados": [],
            "diretorio": str(raiz),
            "completa": False,
            "metodo": None,
            "avisos": ["Informe o nome ou parte do nome do arquivo."],
        }

    avisos: list[str] = []
    caminhos: set[str] = set()
    metodos: list[str] = []

    # -------------------------------------------------------
    # 1. Windows Search
    # -------------------------------------------------------

    encontrados, erros_indice = _buscar_indice(
        termo,
        raiz,
    )

    avisos.extend(erros_indice)

    if encontrados:
        caminhos.update(encontrados)
        metodos.append("windows_search")

    # -------------------------------------------------------
    # 2. Fallback
    # -------------------------------------------------------

    # O índice pode não conter Z:, compartilhamentos ou pastas excluídas.
    # Portanto, ausência no índice NÃO significa ausência no disco.
    encontrados_where, erros_where, busca_completa = _buscar_where(
        termo,
        raiz,
    )

    avisos.extend(erros_where)

    if encontrados_where:
        caminhos.update(encontrados_where)
        metodos.append("where")

    # -------------------------------------------------------
    # Ranking
    # -------------------------------------------------------

    resultados = []

    for caminho in caminhos:
        path = Path(caminho)

        nota = _pontuacao(
            termo,
            path.name,
        )

        if nota <= 0:
            continue

        resultados.append(
            {
                "nome": path.name,
                "caminho": str(path),
                "similaridade": round(nota, 3),
            }
        )

    resultados.sort(
        key=lambda item: (
            -item["similaridade"],
            item["nome"].casefold(),
            item["caminho"].casefold(),
        )
    )

    total = len(resultados)

    if total > MAX_RESULTADOS:
        avisos.append(
            f"Exibindo os {MAX_RESULTADOS} melhores candidatos de {total} encontrados."
        )

    return {
        "resultados": resultados[:MAX_RESULTADOS],
        "diretorio": str(raiz.absolute()),
        "completa": busca_completa,
        "metodo": "+".join(metodos) if metodos else "nenhum",
        "total_encontrados": total,
        "avisos": avisos,
    }


# ---------------------------------------------------------------------------
# Listagem
# ---------------------------------------------------------------------------


def listar_diretorio(caminho: str) -> list[dict]:
    pasta = Path(caminho)

    if not pasta.exists():
        return [
            {
                "erro": "Diretório não encontrado",
                "caminho": str(pasta),
            }
        ]

    if not pasta.is_dir():
        return [
            {
                "erro": "O caminho informado não é um diretório",
                "caminho": str(pasta),
            }
        ]

    try:
        itens = list(pasta.iterdir())

    except OSError as erro:
        return [
            {
                "erro": str(erro),
                "caminho": str(pasta),
            }
        ]

    return [
        {
            "nome": item.name,
            "caminho": str(item),
            "tipo": "diretorio" if item.is_dir() else "arquivo",
        }
        for item in sorted(
            itens,
            key=lambda item: (
                not item.is_dir(),
                item.name.casefold(),
            ),
        )
    ]
