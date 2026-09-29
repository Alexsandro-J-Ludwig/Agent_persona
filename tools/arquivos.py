"""Localiza arquivos por nome sem depender do índice do Windows.

A busca é limitada por tempo e quantidade de arquivos. O retorno informa quando
não foi possível examinar tudo, evitando tratar uma busca parcial como ausência.
"""

import os
import re
import time
import unicodedata
from difflib import SequenceMatcher
from pathlib import Path


def _palavras(texto: str) -> list[str]:
    """Normaliza acentos, caixa e separadores, sem interpretar curingas ou SQL."""
    texto = unicodedata.normalize("NFKD", texto.casefold())
    texto = "".join(c for c in texto if not unicodedata.combining(c))
    return re.findall(r"[^\W_]+", texto)


def _pontuacao(termo: list[str], nome: str) -> float:
    palavras = _palavras(nome)
    if not palavras:
        return 0.0
    notas = []
    for palavra in termo:
        melhor = max(
            1.0 if palavra in candidata else (
                SequenceMatcher(None, palavra, candidata).ratio()
                if len(palavra) >= 4 and len(candidata) >= 4 else 0.0
            )
            for candidata in palavras
        )
        # Todas as palavras precisam combinar; termos curtos exigem trecho literal.
        if melhor < 0.8:
            return 0.0
        notas.append(melhor)
    return sum(notas) / len(notas)


def pesquisar_arquivo(termo: str, diretorio: str | None = None):
    """Busca recursivamente no diretório informado ou na pasta pessoal atual.

    Retorna candidatos, avisos e completude. Similaridade indica um candidato,
    não confirma sua identidade: o agente deve conferir o nome e ler o documento.
    Para pastas grandes, informe um diretório mais específico e repita a busca.
    """
    raiz = Path(diretorio).expanduser() if diretorio else Path.home()
    palavras = _palavras(termo)
    if not palavras:
        return {"resultados": [], "completa": False, "avisos": ["Informe um nome ou palavras do nome do arquivo."]}
    try:
        raiz_disponivel = raiz.is_dir()
    except OSError as erro:
        return {
            "resultados": [], "diretorio": str(raiz), "completa": False,
            "avisos": [f"Não foi possível acessar o diretório: {erro}"],
        }
    if not raiz_disponivel:
        return {"resultados": [], "completa": False, "avisos": [f"Diretório inexistente ou inacessível: {raiz}"]}

    resultados = []
    avisos = []
    examinados = 0
    inicio = time.monotonic()
    limite_arquivos = 50000
    limite_segundos = 15
    limite_resultados = 50

    def erro_acesso(erro):
        if len(avisos) < 10:
            avisos.append(f"Não foi possível acessar {erro.filename}: {erro.strerror}")

    # Não segue links de diretórios para evitar ciclos e sair do escopo solicitado.
    for pasta, subpastas, arquivos in os.walk(raiz, onerror=erro_acesso, followlinks=False):
        subpastas[:] = sorted(
            nome for nome in subpastas
            if not Path(pasta, nome).is_symlink()
            and not (hasattr(os.path, "isjunction") and os.path.isjunction(Path(pasta, nome)))
        )
        for nome in arquivos:
            if examinados >= limite_arquivos or time.monotonic() - inicio >= limite_segundos:
                avisos.append("Limite de busca atingido. Repita informando uma pasta mais específica.")
                break
            examinados += 1
            nota = _pontuacao(palavras, nome)
            if nota:
                resultados.append({"nome": nome, "caminho": str(Path(pasta, nome).absolute()), "similaridade": round(nota, 3)})
        if examinados >= limite_arquivos or time.monotonic() - inicio >= limite_segundos:
            if not avisos or not avisos[-1].startswith("Limite de busca"):
                avisos.append("Limite de busca atingido. Repita informando uma pasta mais específica.")
            break

    resultados.sort(key=lambda item: (-item["similaridade"], item["nome"].casefold(), item["caminho"]))
    completa = not avisos
    if len(resultados) > limite_resultados:
        avisos.append(f"Exibindo os {limite_resultados} melhores candidatos de {len(resultados)} encontrados.")
    return {
        "resultados": resultados[:limite_resultados],
        "diretorio": str(raiz.absolute()),
        "completa": completa,
        "arquivos_examinados": examinados,
        "total_encontrados": len(resultados),
        "avisos": avisos,
    }

def listar_diretorio(caminho: str) -> list[dict]:
    """Lista somente os itens imediatos da pasta; não lê nem busca conteúdo.

    Os campos de retorno são usados pelo agente para navegar em subpastas ou
    passar o caminho completo de um arquivo para ler_documento.
    """
    pasta = Path(caminho)

    if not pasta.exists():
        return [{
            "erro": "Diretório não encontrado",
            "caminho": str(pasta)
        }]

    if not pasta.is_dir():
        return [{
            "erro": "O caminho informado não é um diretório",
            "caminho": str(pasta)
        }]

    return [
        {
            "nome": item.name,
            "caminho": str(item),
            "tipo": "diretorio" if item.is_dir() else "arquivo"
        }
        for item in pasta.iterdir()
    ]
