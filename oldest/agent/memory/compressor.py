from agent.config import MODEL_CONFIG, create_client


class HistoryCompressor:
    def __init__(self):
        self.client = create_client()
        self.model = MODEL_CONFIG["agent"]["model"]

    def comprimir(self, mensagens: list[dict], resumo_anterior: str = "") -> str:

        historico = "\n\n".join(
            f"[ID {m['id']}] {m['role']}:\n{m['content']}" for m in mensagens
        )

        prompt = f"""
            Você é o sistema de compressão de memória de um agente
                
            RESUMO ANTERIOR:
            {resumo_anterior or "Nenhum"}
                
            NOVO HISTÓRICO:
            {historico}
                
            Produza um resumo compacto que preserve:
            - Descisões tomadas;
            - Informações importantes fornecidas pelo usuário;
            - Configurações e valores relevantes;
            - Problemas e soluções encontradas;
            - Tarefas ainda pendentes;
            - Mudanças de decisão;
            - Referências aos IDs das messagens quando úteis.
                
            Remova:
            - Repetições;
            - Conversa casual irrelevante;
            - Tentativas descartadas sem importância futura;
            - Explicações redundantes.
                
            Não invente informações.
            As mensagens originais permanecem armazenadas no banco.
        """

        response = self.client.generate(
            model=self.model,
            prompt=prompt,
        )

        return response.response
