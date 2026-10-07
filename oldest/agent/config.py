"""Parâmetros compartilhados e autenticação. Ajuste modelo e limites aqui.

A criação do cliente é explícita: importar o módulo não exige credenciais.
"""

import os

from dotenv import load_dotenv
from ollama import Client

MODEL_CONFIG = {
    "agent": {
        "model": "qwen3-coder-30b-a3b-instruct:latest",
        "num_ctx": 8192,
        "keep_alive": "30m",
    },
    "think": {
        "model": "qwen3-next-80b-a3b-thinking:latest",
        "num_ctx": 16384,
        "keep_alive": 0,
    },
    "multimodal": {
        "model": "gemma4:e4b",
        "num_ctx": 8192,
        "keep_alive": "5m",
    },
    "tts": {"model": "kokoro:latest", "keep_alive": "5m"},
}

HOST = "https://api.incubebots.com"
NETWORK_ROOT = "Z:/"  # Unidade de rede apresentada ao modelo em tools/schemas.py.
# Mantém o modelo carregado entre mensagens próximas.

MAX_TOOL_ROUNDS = 20
MAX_HISTORY_MESSAGES = 32

# Numero de bytes permitidas para leitura doo modelo
MAX_INPUT_BYTES = 49152

# Usa o identificador configurado para evitar divergência entre prompt e modelo.
SYSTEM_PROMPT = """
Você é um assistente de IA local com acesso às ferramentas fornecidas.
Responda com frases curtas sem texto discursivo. Fale frases curtas e mais explicativas. Sem enrolar

REGRAS DE FERRAMENTAS:

- Quando decidir utilizar uma ferramenta, execute-a imediatamente.
  
- Após receber o resultado de uma ferramenta, continue trabalhando
  automaticamente até concluir a solicitação.

- Para perguntas sobre documentos, use buscar_documento quando houver um termo
  específico. Para ler arquivos longos, use ler_documento em blocos e acompanhe
  proximo_inicio; tem_mais indica que ainda há partes não examinadas.
  
- Nunca afirme ter analisado um documento inteiro após ler só alguns blocos.

- Somente encerre quando a tarefa estiver concluída ou quando realmente
  precisar de informação adicional do usuário.

- Nunca invente ou deduza a existência de caminhos, arquivos ou diretórios.

- Um caminho só pode ser tratado como existente se tiver sido retornado ou
  confirmado por uma ferramenta.

- Não trate conhecimento geral sobre a estrutura de um programa como evidência
  de que um arquivo existe neste computador.

- Se uma hipótese sobre a localização de um arquivo falhar, não repita a mesma
  hipótese com pequenas variações sem nova evidência.

- Após várias tentativas sem progresso, pare a investigação e informe ao
  usuário o que foi verificado e que o recurso não foi localizado.

- Não continue chamando ferramentas apenas para consumir o limite de chamadas.
  Cada nova chamada deve ser baseada em informação obtida anteriormente.
  
- Você tem capacidade de realizar multiplas chamadas de ferramentas de forma assincrona em paralelo

-Quando várias chamadas de ferramentas forem independentes entre si, solicite-as
  na mesma resposta para permitir execução paralela. Não crie uma etapa separada
  para cada busca equivalente.

  Exemplo: se precisar pesquisar o mesmo arquivo em cinco diretórios independentes,
  faça as cinco chamadas de pesquisar_arquivo na mesma resposta.

  Use execução sequencial apenas quando uma chamada depender do resultado da anterior.
""".strip()


def create_client() -> Client:
    """Lê o .env e valida as credenciais ao iniciar a aplicação."""
    load_dotenv()

    client_id = os.getenv("CF-Access-Client-Id")
    client_secret = os.getenv("CF-Access-Client-Secret")

    if not client_id or not client_secret:
        raise ValueError(
            "As variáveis CF-Access-Client-Id e "
            "CF-Access-Client-Secret não foram configuradas."
        )

    return Client(
        host=HOST,
        headers={
            "CF-Access-Client-Id": client_id,
            "CF-Access-Client-Secret": client_secret,
        },
    )
