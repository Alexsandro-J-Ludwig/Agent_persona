import os
import xml.etree.ElementTree as ET
from rich.console import Console
from rich.table import Table
from dotenv import load_dotenv
import requests

load_dotenv()

console = Console()

user_name = os.getenv("USER")
user_pass = os.getenv("PASS")

endpoint = (
    "http://snrsj:8080/g5-senior-services/rubi_Synccom.senior.g5.rh.fp.USUColaborador"
)
header = {"Content-Type": "text/xml; charset=utf-8"}

ultima_consulta = []


def consultar_colaborador(matricula: str) -> str:
    """Executa a chamada real."""
    numemp = matricula[0]

    payload = f"""
        <?xml version="1.0" encoding="UTF-8"?>
            <soapenv:Envelope
                xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
                xmlns:ser="http://services.senior.com.br">
            <soapenv:Header/>
            <soapenv:Body>
                <ser:ConsultarColaborador>
                <user>{user_name}</user>
                <password>{user_pass}</password>
                <encryption>0</encryption>
                <parameters>
                    <numEmp>{numemp}</numEmp>
                    <tipCol>1</tipCol>
                    <numCad>{matricula}</numCad>
                </parameters>
                </ser:ConsultarColaborador>
            </soapenv:Body>
            </soapenv:Envelope>
        """

    resp = requests.post(url=endpoint, data=payload, headers=header)

    resp.raise_for_status()
    return resp.text


def consultar_colaboradores_ativos(DatIni: str, DatFim: str) -> tuple[str, str]:
    import re

    padrao = r"^\d{2}/\d{2}/\d{4}$"

    if not re.match(padrao, DatIni) or not re.match(padrao, DatFim):
        erro = "Erro: datas devem estar no formato DD/MM/AAAA."
        return erro, erro

    payload = f"""
    <?xml version="1.0" encoding="UTF-8"?>
    <soapenv:Envelope
        xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
        xmlns:ser="http://services.senior.com.br">
        <soapenv:Header/>
        <soapenv:Body>
            <ser:ConsultarColaboradoresAtivos>
                <user>{user_name}</user>
                <password>{user_pass}</password>
                <encryption>0</encryption>
                <parameters>
                    <DatIni>{DatIni}</DatIni>
                    <DatFim>{DatFim}</DatFim>
                </parameters>
            </ser:ConsultarColaboradoresAtivos>
        </soapenv:Body>
    </soapenv:Envelope>
    """

    resp = requests.post(url=endpoint, data=payload, headers=header)

    resp.raise_for_status()

    root = ET.fromstring(resp.text)

    resultado_tag = root.find(".//resultado")

    if resultado_tag is None or resultado_tag.text != "OK":
        erro = root.find(".//erroExecucao")
        mensagem = (
            f"Erro na consulta: "
            f"{erro.text if erro is not None and erro.text else 'erro desconhecido'}"
        )
        return mensagem, mensagem

    retornos = root.findall(".//retorno")
    total = len(retornos)

    if total == 0:
        mensagem = (
            f"Nenhum colaborador ativo encontrado no período {DatIni} a {DatFim}."
        )
        return mensagem, mensagem

    # -------------------------------------------------------
    # CACHE COMPLETO — não vai para o Qwen
    # -------------------------------------------------------

    ultima_consulta.clear()

    for r in retornos:
        ultima_consulta.append(
            {
                "Empresa": r.findtext("numEmp", "").strip(),
                "matricula": r.findtext("numCad", "").strip(),
                "nome": r.findtext("nomFun", "").strip(),
                "CPF": r.findtext("numCpf", "").strip(),
                "Posto de Trabalho": r.findtext("posTra", "").strip(),
                "cargo": r.findtext("titRed", "").strip(),
                "Codigo da Filial": r.findtext("codFil", "").strip(),
                "filial": r.findtext("nomFil", "").strip(),
                "admissao": r.findtext("datAdm", "").strip(),
                "Centro de custo": r.findtext("codCcu", "").strip(),
                "Nome do Centro de Custo": r.findtext("nomCcu", "").strip(),
                "Codigo do Gestor": r.findtext("codGes", "").strip(),
                "Nome do Gestor": r.findtext("nomGes", "").strip(),
                "Email do Gestor": r.findtext("emaGes", "").strip(),
            }
        )

    # -------------------------------------------------------
    # 1. CONTEÚDO QUE ENTRA NO CONTEXTO DO QWEN
    # -------------------------------------------------------

    LIMITE_CONTEXTO = 25

    memoria = [
        f"Consulta realizada com sucesso.",
        f"Período: {DatIni} a {DatFim}.",
        f"Total de colaboradores encontrados: {total}.",
        "",
        f"Primeiros {min(LIMITE_CONTEXTO, total)} colaboradores:",
    ]

    for indice, colaborador in enumerate(ultima_consulta[:LIMITE_CONTEXTO], start=1):
        memoria.append(
            f"{indice}. "
            f"{colaborador['matricula']} | "
            f"{colaborador['nome']} | "
            f"{colaborador['cargo']} | "
            f"{colaborador['filial']}"
        )

    if total > LIMITE_CONTEXTO:
        memoria.append(
            f"\nExistem mais {total - LIMITE_CONTEXTO} registros "
            f"armazenados fora do contexto. "
            f"Não invente informações sobre registros não presentes acima. "
            f"Use uma ferramenta de consulta quando precisar acessá-los."
        )

    memoria_qwen = "\n".join(memoria)

    # -------------------------------------------------------
    # 2. CONTEÚDO EXIBIDO DIRETAMENTE AO USUÁRIO
    # -------------------------------------------------------

    console.print(f"\n[bold cyan]Total de colaboradores: {total}[/bold cyan]\n")

    table = Table(
        title=f"Colaboradores Ativos — {DatIni} a {DatFim}",
        show_header=True,
        header_style="bold cyan",
        show_lines=False,
    )

    table.add_column("#", justify="right")
    table.add_column("Empresa")
    table.add_column("Matrícula")
    table.add_column("Nome")
    table.add_column("CPF")
    table.add_column("Posto")
    table.add_column("Cargo")
    table.add_column("Cód. Filial")
    table.add_column("Filial")
    table.add_column("Admissão")
    table.add_column("Cód. C.Custo")
    table.add_column("Centro de Custo")
    table.add_column("Cód. Gestor")
    table.add_column("Gestor")
    table.add_column("Email Gestor")

    for indice, colaborador in enumerate(ultima_consulta, start=1):
        table.add_row(
            str(indice),
            colaborador["Empresa"],
            colaborador["matricula"],
            colaborador["nome"],
            colaborador["CPF"],
            colaborador["Posto de Trabalho"],
            colaborador["cargo"],
            colaborador["Codigo da Filial"],
            colaborador["filial"],
            colaborador["admissao"],
            colaborador["Centro de custo"],
            colaborador["Nome do Centro de Custo"],
            colaborador["Codigo do Gestor"],
            colaborador["Nome do Gestor"],
            colaborador["Email do Gestor"],
        )

    console.print()
    console.print(table)
    console.print(
        f"\n[bold cyan]Total de colaboradores encontrados: {total}[/bold cyan]"
    )

    return (
        f"Consulta concluída com sucesso para o período "
        f"{DatIni} a {DatFim}. "
        f"Foram encontrados {total} colaboradores. "
        f"A listagem completa com os {total} registros já foi "
        f"exibida diretamente ao usuário pela ferramenta. "
        f"Não repita a listagem."
    )
