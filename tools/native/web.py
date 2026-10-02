# Pesquisa na internet
def search(consulta: str) -> list[dict]:
    import requests
    import os
    from dotenv import load_dotenv

    load_dotenv()

    headers = {
        "CF-Access-Client-Id": os.getenv("CF-Access-Client-Id"),
        "CF-Access-Client-Secret": os.getenv(
            "cfast_WRbG9KWpNXAgOQCFePNiSXzzeep9klJZO224mIB9757d79d0"
        ),
    }

    response = requests.get(
        "http://searxmg:8080/search",
        headers=headers,
        params={"q": consulta, "format": "json"},
        timeout=10,
    )

    response.raise_for_status()

    dados = response.json()

    return [
        {
            "Título": resultado.get("title"),
            "url": resultado.get("url"),
            "resumo": resultado.get("content"),
        }
        for resultado in dados["results"][:10]
    ]


def available_risk(url: str) -> dict:
    import os

    import requests

    api_key = os.getenv("GOOGLE_WEBRISK_API_KEY")

    if not api_key:
        return {"erro": "GOOGLE_WEBRISK_API_KEY não configurada."}

    endpoint = "https://webrisk.googleapis.com/v1/uris:search"

    params = [
        ("uri", url),
        ("threatTypes", "MALWARE"),
        ("threatTypes", "SOCIAL_ENGINEERING"),
        ("threatTypes", "UNWANTED_SOFTWARE"),
        ("key", api_key),
    ]

    try:
        response = requests.get(endpoint, params=params, timeout=10)

        response.raise_for_status()

        resultado = response.json()

        if resultado.get("threat"):
            return {"seguro": False, "url": url, "ameacas": resultado["threat"]}

        return {"seguro": True, "url": url, "ameacas": []}

    except requests.RequestException as e:
        return {"seguro": None, "url": url, "erro": str(e)}


def resume_page(conteudo: str) -> dict:
    from agent.config import MODEL_CONFIG, create_client

    client = create_client()

    prompt = f"""
        Você é um componente auxiliar de outro agente de IA.

        Analise o conteúdo abaixo e produza uma representação compacta e fiel
        para ser utilizada pelo agente principal.

        Regras:
        - Preserve fatos, nomes, números, datas, versões e URLs relevantes.
        - Não invente informações.
        - Remova conteúdo redundante ou irrelevante.
        - Organize as informações de maneira que outro modelo consiga utilizá-las.
        - Não converse com o usuário.
        - Não descreva o que você está fazendo.
        - Retorne somente o conteúdo processado.

        CONTEÚDO DA PÁGINA:

        {conteudo}
        """

    response = client.chat(
        model=MODEL_CONFIG[1],
        messages=[{"role": "user", "content": prompt}],
        think=True,
        keep_alive=MODEL_CONFIG[1],
        options={"num_ctx": MODEL_CONFIG[1]},
    )

    return {"resumo_pagina": response.message.content}


def read_page(url: str) -> dict | str:
    from bs4 import BeautifulSoup
    import requests

    risco = available_risk(url)

    if risco.get("seguro") is not True:
        return {"erro": "A URL não foi liberada para acesso.", "verificacao": risco}

    try:
        headers = {
            "User-Agent": (
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
            )
        }

        resposta = requests.get(url, headers=headers, timeout=10)

        resposta.raise_for_status()

        content_type = resposta.headers.get("Content-Type", "")

        if "text/html" not in content_type:
            return f"Conteúdo não suportado: {content_type}"

        soup = BeautifulSoup(resposta.text, "html.parser")

        for elemento in soup(
            ["script", "style", "nav", "footer", "header", "noscript"]
        ):
            elemento.decompose()

        texto = soup.get_text(separator=" ", strip=True)

        content = resume_page(texto)

        return {"Pesquisa": content}

    except requests.Timeout:
        return "Erro ao ler página: tempo limite excedido."

    except requests.RequestException as e:
        return f"Erro HTTP ao ler página: {e}"

    except Exception as e:
        return f"Erro ao processar página: {e}"
