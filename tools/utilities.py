"""
Funções genéricas e pequenas para uso rotineiro do agente

- Pegar dia de hoje (dd/mm/yyyy, hh:mm:ss)
- Calcular que dia da semana foi determinado dia
- Pesquisar conteúdo
"""


# Retorna a data e horario de hoje
def today() -> dict:
    import datetime

    agora = datetime.datetime.now()

    dias = [
        "segunda-feira",
        "terça-feira",
        "quarta-feira",
        "quinta-feira",
        "sexta-feira",
        "sábado",
        "domingo",
    ]

    return {
        "data": agora.strftime("%d/%m/%Y"),
        "hora": agora.strftime("%H:%M:%S"),
        "dia_semana": dias[agora.weekday()],
    }


# Retorna o dia da semana da data informada
def date_previous(data: str) -> dict:
    import datetime

    data_informada = datetime.datetime.strptime(data, "%d/%m/%Y").date()

    dias = [
        "segunda-feira",
        "terça-feira",
        "quarta-feira",
        "quinta-feira",
        "sexta-feira",
        "sábado",
        "domingo",
    ]

    dia_semana = dias[data_informada.weekday()]

    return {"Dia da semana": dia_semana}


# Retorna a data com diferença de dias informado
def calc_data(dias: int, data: str) -> dict:
    import datetime

    data_informada = datetime.datetime.strptime(data, "%d/%m/%Y").date()

    data_encontrada = data_informada + datetime.timedelta(days=dias)

    return {"data": data_encontrada.strftime("%d/%m/%Y")}
