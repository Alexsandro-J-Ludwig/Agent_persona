import sqlite3
from datetime import datetime
from pathlib import Path
from uuid import uuid4


class Database:
    def __init__(self):
        self.caminho = self.caminho = Path.home() / ".incube_agent" / "memory.db"
        self.caminho.parent.mkdir(parents=True, exist_ok=True)

        self.__inicializar()
        self.chat_id = self.criar_chat()

    def conectar(self) -> sqlite3.Connection:
        conexao = sqlite3.connect(self.caminho)

        conexao.row_factory = sqlite3.Row
        conexao.execute("PRAGMA foreign_keys = ON")

        return conexao

    def __inicializar(self):
        with self.conectar() as db:
            db.executescript("""
                CREATE TABLE IF NOT EXISTS chats (
                    id TEXT PRIMARY KEY,
                    titulo TEXT,
                    resumo TEXT,
                    criado_em TEXT NOT NULL,
                    atualizado_em TEXT NOT NULL
                );
                
                CREATE TABLE IF NOT EXISTS messages (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    chat_id TEXT NOT NULL,
                    role TEXT NOT NULL,
                    content TEXT NOT NULL,
                    criado_em TEXT NOT NULL,
                    
                    FOREIGN KEY (chat_id)
                        REFERENCES chats(id)
                        ON DELETE CASCADE
                );
                
                CREATE INDEX IF NOT EXISTS idx_messages_chat
                ON messages(chat_id, id);
                
            """)

    def criar_chat(self):
        agora = datetime.now().isoformat()
        chat_id = str(uuid4())

        with self.conectar() as db:
            db.execute(
                """
                    INSERT INTO chats (
                        id,
                        criado_em,
                        atualizado_em
                    )
                    VALUES (?, ?, ?)
                """,
                (chat_id, agora, agora),
            )

            return chat_id

    def salvar_mensagem(self, role: str, content: str):
        agora = datetime.now().isoformat()

        with self.conectar() as db:
            db.execute(
                """
                    INSERT INTO messages (
                        chat_id,
                        role,
                        content, 
                        criado_em
                    )
                    VALUES (?, ?, ?, ?)
                """,
                (self.chat_id, role, content, agora),
            )

            db.execute(
                """
                    UPDATE chats
                    SET atualizado_em = ?
                    WHERE id = ?
                """,
                (agora, self.chat_id),
            )

    def lembrar_conversa(
        self,
        content: str,
        role: str | None = None,
        limit: int = 30,
    ) -> str:
        from agent.config import MODEL_CONFIG, create_client

        client = create_client()

        with self.conectar() as db:
            cursor = db.execute(
                """
                SELECT role, content
                FROM messages
                WHERE (? IS NULL OR role = ?)
                AND (? IS NULL OR content LIKE ?)
                ORDER BY id DESC
                LIMIT ?""",
                (role, content, f"%{content}%" if content is not None else None, limit),
            )

            mensagens = cursor.fetchall()

        prompt = f"""
            Você é uma ferramente auxiliar de um agente de IA
            
            Sua responsabilidade é resumir o hisórico de uma conversa anterior
            preservando os principais acontecimentos e contexto da conversa.
            
            PRESERVE:
            - Tópicos
            - Contexto
            - Infiormações importantes
            
            MENSAGENS:
            {mensagens}
        """

        response = client.generate(
            model=MODEL_CONFIG["think"]["model"],
            prompt=prompt,
            keep_alive=MODEL_CONFIG["think"]["keep_alive"],
            options={"num_ctx": MODEL_CONFIG["think"]["num_ctx"]},
        )

        return response.response
