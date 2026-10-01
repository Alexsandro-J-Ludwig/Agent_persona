tools = [
    {
        "type": "function",
        "function": {
            "name": "consultar_colaborador",
            "description": "Consulta uma API externa e retorna a resposta. Use quando o usuário informar uma matricula de 9 digitos e falando sobre e você não sabe das informações.",
            "parameters": {
                "type": "object",
                "properties": {
                    "matricula": {
                        "type": "string",
                        "description": "Matricula a ser consultada",
                    }
                },
                "required": ["matricula"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "consultar_colaboradores_ativos",
            "description": """
                Consulta todos os colaboradores ativos em um determinado período.

                Use somente quando o usuário solicitar a consulta e informar
                as datas inicial e final. Caso falte alguma das datas, solicite-a
                ao usuário antes de chamar esta ferramenta.

                A própria ferramenta exibe a listagem completa diretamente ao usuário.
                O retorno desta ferramenta serve apenas como contexto para você.

                NÃO repita a listagem de colaboradores após executar a ferramenta.
                Apenas confirme a conclusão da consulta ou responda a perguntas
                posteriores sobre os resultados.
                """,
            "parameters": {
                "type": "object",
                "properties": {
                    "DatIni": {
                        "type": "string",
                        "description": "Periodo inicial da consulta",
                    },
                    "DatFim": {
                        "type": "string",
                        "description": "Periodo final da consulta",
                    },
                },
                "required": ["DatIni", "DatFim"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "pesquisar_arquivo",
            "description": """
                Busca arquivos por palavras do nome, ignorando acentos, caixa e separadores,
                com tolerância a pequenos erros de digitação. Não pesquisa o conteúdo.
                Informe somente o nome ou palavras relevantes, sem frases como 'encontre o arquivo'.
                O diretório padrão é a pasta pessoal do usuário; use diretorio quando conhecer a pasta.
                Confira resultados e avisos: completa=false não significa que o arquivo não existe.
                Em buscas parciais, repita em uma pasta mais específica.
                Use ler_documento com o caminho retornado antes de analisar o conteúdo.
                Se houver candidatos ambíguos, peça ao usuário para identificar o desejado.
            """,
            "parameters": {
                "type": "object",
                "properties": {
                    "termo": {
                        "type": "string",
                        "description": "Nome completo ou palavras do nome do arquivo",
                    },
                    "diretorio": {
                        "type": "string",
                        "description": """
                            Pasta raiz onde realizar a busca recursiva.
                            Opcional; se omitido, pesquisa na pasta pessoal do usuário.

                            Raízes disponíveis:
                            - C:/ — disco local
                            - Z:/ — unidade de rede corporativa

                            Use Z:/ somente quando houver motivo para acreditar que o arquivo
                            está na rede. Quando conhecer uma subpasta mais específica,
                            prefira-a em vez de pesquisar toda a unidade.
                        """,
                    },
                },
                "required": ["termo"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "ler_documento",
            "description": """
            Lê um trecho pequeno de um documento localizado no computador.
            Use esta função quando o caminho do arquivo já for conhecido,
            normalmente após utilizar a função pesquisar_arquivo.
            O retorno informa proximo_inicio quando ainda houver conteúdo.
            Para continuar, chame novamente com esse valor em inicio.
            Se a pergunta for específica, prefira buscar_documento para localizar
            apenas os trechos relevantes. Não conclua sobre partes não lidas.
        """,
            "parameters": {
                "type": "object",
                "properties": {
                    "caminho": {
                        "type": "string",
                        "description": "Caminho completo do arquivo que será lido",
                    },
                    "inicio": {
                        "type": "integer",
                        "description": "Posição em caracteres retornada em proximo_inicio. Padrão: 0.",
                    },
                    "tamanho": {
                        "type": "integer",
                        "description": "Máximo de caracteres no trecho, de 1 a 4000. Padrão: 2500.",
                    },
                },
                "required": ["caminho"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "buscar_documento",
            "description": """
            Procura uma expressão dentro de PDF, DOCX, XLSX, XLSM, CSV,
            TXT, MD, JSON e py sem enviar o arquivo completo ao modelo.
            Retorna trechos e posições. Use quando a pergunta citar um assunto,
            nome ou termo específico. Se busca_completa for falsa, os resultados
            podem ser parciais. Para contexto adicional, chame ler_documento
            com o inicio de um trecho retornado.
        """,
            "parameters": {
                "type": "object",
                "properties": {
                    "caminho": {
                        "type": "string",
                        "description": "Caminho completo do documento",
                    },
                    "termo": {
                        "type": "string",
                        "description": "Expressão textual a encontrar no conteúdo",
                    },
                    "limite": {
                        "type": "integer",
                        "description": "Máximo de trechos, de 1 a 20. Padrão: 8.",
                    },
                },
                "required": ["caminho", "termo"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "listar_diretorio",
            "description": """
            Lista os arquivos e subdiretórios existentes dentro de uma pasta
            do computador.

            Use esta ferramenta quando o usuário pedir para listar, verificar,
            explorar ou examinar os arquivos existentes em um diretório.

            Esta ferramenta NÃO lê o conteúdo dos arquivos. Ela retorna apenas
            informações sobre os itens encontrados, incluindo nome, caminho
            completo e se o item é um arquivo ou diretório.

            Se for necessário analisar o conteúdo de um arquivo encontrado,
            utilize posteriormente a ferramenta ler_documento com o caminho
            retornado por esta ferramenta.

            Para explorar uma subpasta encontrada, esta ferramenta pode ser
            chamada novamente utilizando o caminho da subpasta.
        """,
            "parameters": {
                "type": "object",
                "properties": {
                    "caminho": {
                        "type": "string",
                        "description": """
                        Caminho completo do diretório que será listado.
                    """,
                    }
                },
                "required": ["caminho"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "search",
            "description": """
            Pesquisa informações na internet.

            Use quando o usuário solicitar uma pesquisa na web ou quando
            informações atuais, externas ou não disponíveis no conhecimento
            do modelo forem necessárias.

            Retorna até 10 resultados contendo título, URL e um pequeno
            resumo fornecido pelo mecanismo de busca.

            O resultado da pesquisa NÃO significa que a página foi lida.
            Quando for necessário verificar ou analisar uma fonte,
            utilize read_page com a URL retornada.

            Prefira consultar fontes relevantes antes de responder sobre
            informações atuais.
        """,
            "parameters": {
                "type": "object",
                "properties": {
                    "consulta": {
                        "type": "string",
                        "description": "Consulta que será enviada ao mecanismo de busca.",
                    }
                },
                "required": ["consulta"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "read_page",
            "description": """
            Acessa e analisa uma página encontrada na internet.

            Use esta ferramenta quando precisar verificar o conteúdo de uma
            URL ou aprofundar um resultado retornado por search.

            A URL passa por uma verificação de segurança antes do acesso.
            A página é baixada, seu HTML é convertido em texto e seu conteúdo
            é analisado por um modelo auxiliar. O agente principal recebe uma
            representação compacta da página em vez do HTML completo.

            Não afirme que uma página foi lida apenas porque apareceu nos
            resultados de search. Considere a página analisada somente após
            read_page retornar com sucesso.
        """,
            "parameters": {
                "type": "object",
                "properties": {
                    "url": {
                        "type": "string",
                        "description": "URL completa da página que será analisada.",
                    }
                },
                "required": ["url"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "today",
            "description": """
            Retorna a data e o horário atuais do computador, incluindo
            o dia da semana.

            Use esta ferramenta quando for necessário saber a data atual,
            horário atual ou dia da semana atual.

            Não estime essas informações usando conhecimento próprio,
            pois a ferramenta consulta o relógio do sistema.
        """,
            "parameters": {"type": "object", "properties": {}, "required": []},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "date_previous",
            "description": """
            Determina o dia da semana correspondente a uma data informada.

            Use quando for necessário descobrir em qual dia da semana
            determinada data caiu ou cairá.

            A data deve ser fornecida no formato DD/MM/AAAA.
        """,
            "parameters": {
                "type": "object",
                "properties": {
                    "data": {
                        "type": "string",
                        "description": """
                        Data que será consultada no formato DD/MM/AAAA.
                        Exemplo: 28/09/2026.
                    """,
                    }
                },
                "required": ["data"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "calc_data",
            "description": """
            Calcula uma nova data adicionando ou subtraindo uma quantidade
            de dias de uma data de referência.

            Use um número positivo para calcular dias posteriores e um
            número negativo para calcular dias anteriores.

            Exemplos:
            dias=7 adiciona sete dias.
            dias=-7 subtrai sete dias.

            A data de referência deve estar no formato DD/MM/AAAA.
            Caso seja necessário calcular em relação à data atual,
            utilize primeiro a ferramenta today para descobrir a data.
        """,
            "parameters": {
                "type": "object",
                "properties": {
                    "dias": {
                        "type": "integer",
                        "description": """
                        Quantidade de dias que será adicionada ou subtraída.
                        Valores positivos avançam no tempo e valores negativos
                        retrocedem.
                    """,
                    },
                    "data": {
                        "type": "string",
                        "description": """
                        Data de referência no formato DD/MM/AAAA.
                        Exemplo: 28/09/2026.
                    """,
                    },
                },
                "required": ["dias", "data"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "lembrar_conversa",
            "description": (
                "Busca e recupera informações de conversas anteriores armazenadas "
                "na memória persistente do agente. Use quando o usuário mencionar "
                "algo discutido anteriormente, pedir para lembrar uma conversa, "
                "decisão, informação, projeto ou contexto passado. "
                "A busca atualmente é textual, portanto forneça em 'content' "
                "palavras-chave curtas e relevantes que provavelmente apareceram "
                "na conversa original."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "content": {
                        "type": "string",
                        "description": (
                            "Texto ou palavras-chave para localizar mensagens antigas. "
                            "Prefira termos específicos, nomes de projetos, tecnologias, "
                            "pessoas, arquivos ou assuntos, em vez de frases longas."
                        ),
                    },
                    "role": {
                        "type": ["string", "null"],
                        "enum": ["user", "assistant", None],
                        "description": (
                            "Opcional. Restringe a busca às mensagens do usuário ou "
                            "do assistente. Use null quando a origem da informação "
                            "não importar."
                        ),
                    },
                    "limit": {
                        "type": "integer",
                        "minimum": 1,
                        "maximum": 50,
                        "default": 30,
                        "description": (
                            "Quantidade máxima de mensagens antigas que podem ser "
                            "recuperadas antes da sumarização."
                        ),
                    },
                },
                "required": ["content"],
            },
        },
    },
]
