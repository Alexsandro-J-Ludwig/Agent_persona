"""Estado do plano, ferramentas internas e painel de progresso do terminal.

TaskManager guarda o estado real. O painel e o contexto do modelo são derivados
 dele, para que a tela não mostre uma conclusão diferente da usada pelo agente.
"""
from dataclasses import dataclass, field
from typing import Literal

from rich.console import Console, Group
from rich.live import Live
from rich.markdown import Markdown
from rich.panel import Panel
from rich.spinner import Spinner
from rich.table import Table
from rich.text import Text

TaskStatus = Literal["pending", "in_progress", "completed", "failed", "blocked"]
LABELS = {
    "pending": ("Pendente", "dim"),
    "in_progress": ("Em andamento", "cyan"),
    "completed": ("Concluída", "green"),
    "failed": ("Falhou", "red"),
    "blocked": ("Bloqueada", "yellow"),
}


@dataclass
class Task:
    id: int
    description: str
    status: TaskStatus = "pending"
    result: str | None = None


@dataclass
class TaskManager:
    tasks: list[Task] = field(default_factory=list)

    def create(self, descriptions: list[str]) -> None:
        """Um plano não pode apagar tarefas existentes no mesmo turno."""
        if self.tasks:
            raise ValueError("Já existe um plano. Atualize as tarefas existentes.")
        
        if not isinstance(descriptions, list) or not 1 <= len(descriptions) <= 50:
            raise ValueError("Informe uma lista de 1 a 50 tarefas.")
        
        if any(not isinstance(d, str) or not d.strip() or len(d) > 200 for d in descriptions):
            raise ValueError("Cada tarefa precisa de uma descrição de 1 a 200 caracteres.")
        
        self.tasks = [Task(i + 1, text.strip()) for i, text in enumerate(descriptions)]

    def next(self) -> Task | None:
        return next((t for t in self.tasks if t.status == "in_progress"), None) or next(
            (t for t in self.tasks if t.status == "pending"), None,
        )

    def start(self, task_id: int) -> None:
        task = self._get(task_id)
        
        if task.status == "completed":
            raise ValueError("Uma tarefa concluída não pode ser reiniciada neste plano.")
        
        if any(t.status == "in_progress" and t.id != task_id for t in self.tasks):
            raise ValueError("Finalize ou bloqueie a tarefa em andamento antes de iniciar outra.")
        
        task.status, task.result = "in_progress", None

    def complete(self, task_id: int, result: str | None = None) -> None:
        task = self._get(task_id)
        
        if task.status not in ("in_progress", "completed"):
            raise ValueError("Inicie a tarefa antes de concluí-la.")
        
        self._finish(task, "completed", result)

    def fail(self, task_id: int, result: str | None = None) -> None:
        self._finish(self._get(task_id), "failed", result)

    def block(self, task_id: int, result: str | None = None) -> None:
        self._finish(self._get(task_id), "blocked", result)

    def _finish(self, task: Task, status: TaskStatus, result: str | None) -> None:
        if not isinstance(result, str) or not result.strip():
            raise ValueError("Informe o resultado ou o motivo da tarefa.")
        
        if task.status == "completed" and status != "completed":
            raise ValueError("A tarefa já está concluída.")
        
        task.status = status  # Sem vírgula: o status deve ser uma string, nunca tupla.
        task.result = result.strip()[:2000]

    def block_pending(self, reason: str) -> None:
        """Uma interrupção fica visível; nunca converte pendências em sucesso."""
        for task in self.tasks:
            if task.status in ("pending", "in_progress"):
                self.block(task.id, reason)

    def has_pending(self) -> bool:
        return any(t.status in ("pending", "in_progress") for t in self.tasks)

    def clear(self) -> None:
        self.tasks.clear()

    def context(self) -> str:
        return "\n".join(
            f"{t.id}. [{t.status}] {t.description}"
            + (f" — {t.result}" if t.result else "")
            for t in self.tasks
        )

    def _get(self, task_id: int) -> Task:
        if isinstance(task_id, bool) or not isinstance(task_id, int):
            raise ValueError("task_id deve ser um número inteiro.")
        
        task = next((t for t in self.tasks if t.id == task_id), None)
        
        if task is None:
            raise ValueError(f"Tarefa {task_id} não existe.")
        
        return task

    def render(self) -> Panel:
        table = Table.grid(padding=(0, 1), expand=True)
        table.add_column(style="dim", width=3)
        table.add_column()
        table.add_column(width=13)
        for task in self.tasks:
            
            label, color = LABELS[task.status]
            description = Text(task.description)
            
            if task.result:
                description.append("\n" + task.result, style="dim")
                
            table.add_row(str(task.id), description, Text(label, style=color))
            
        completed = sum(t.status == "completed" for t in self.tasks)
        return Panel(table, title=f"Tarefas · {completed}/{len(self.tasks)} concluídas", border_style="cyan")


class TaskDisplay:
    """Um único Live coordena tarefas e streaming, evitando painéis concorrentes.

    No terminal o painel é atualizado no lugar. Em logs/redirecionamento,
    imprimimos snapshots apenas quando o estado muda, sem códigos de cursor.
    """
    def __init__(self, manager: TaskManager, console: Console):
        self.manager, self.console = manager, console
        self.body = Text("")
        self.live = None
        self.last_state = ""

    def __enter__(self):
        if self.console.is_terminal:
            self.live = Live(console=self.console, get_renderable=self.render,
                             refresh_per_second=10, transient=True)
            
            self.live.start()
            
        self.refresh()
        return self

    def render(self):
        parts = [self.body]
        
        if self.manager.tasks:
            parts.append(self.manager.render())
            
        return Group(*parts)

    def refresh(self):
        if self.live:
            self.live.refresh()
            
        else:
            state = self.manager.context()
            
            if state and state != self.last_state:
                self.console.print(self.manager.render())
                
            self.last_state = state

    def status(self, message: str):
        self.body = Spinner("dots", text=Text(message), style="cyan")
        self.refresh()

    def response(self, content: str):
        self.body = Markdown(content)
        self.refresh()

    def finish_response(self, content: str):
        self.body = Text("")
        
        if content:
            self.console.print(Markdown(content))
            
        self.refresh()

    def __exit__(self, *exc):
        self.body = Text("")
        
        if self.live:
            self.live.stop()
            
            if self.manager.tasks:
                self.console.print(self.manager.render())
        else:
            self.refresh()


# Publicadas ao modelo por streaming.py; a execução depende da sessão em Agent.
TASK_TOOLS = [{
    "type": "function",
    "function": {
        "name": "create_tasks",
        "description": """
            Cria o plano de execução e exibe suas tarefas ao usuário no terminal.
            Use em qualquer pedido com várias etapas, não apenas em /verify.
            Chame antes de começar quando precisar localizar e analisar arquivos,
            comparar documentos, pesquisar e conferir fontes ou executar ações
            que dependam dos resultados de ferramentas anteriores.
            Divida o objetivo em etapas concretas e verificáveis. Criar o plano
            não executa as etapas: use start_task, execute as ferramentas e
            registre o resultado com complete_task, fail_task ou block_task.
            Use uma vez por solicitação; não recrie um plano já existente.
            Respostas diretas e consultas simples de uma etapa não precisam de plano.
        """,
        "parameters": {"type": "object", "properties": {
            "tarefas": {"type": "array", "items": {"type": "string"}, "minItems": 1, "maxItems": 50},
        }, "required": ["tarefas"]},
    },
}]

for name, description in (
    ("start_task", "Inicia uma tarefa do plano antes de executar sua ação."),
    ("complete_task", "Conclui uma tarefa iniciada, somente após conferir o resultado real."),
    ("fail_task", "Marca uma tarefa como falha e informa o erro. Continue as demais tarefas independentes."),
    ("block_task", "Marca uma tarefa bloqueada por falta de dados ou acesso e informa o motivo."),
):
    properties = {"task_id": {"type": "integer"}}
    required = ["task_id"]
    if name != "start_task":
        properties["resultado"] = {"type": "string", "description": "Resultado observado ou motivo específico."}
        required.append("resultado")
    TASK_TOOLS.append({"type": "function", "function": {
        "name": name, "description": description,
        "parameters": {"type": "object", "properties": properties, "required": required},
    }})
TASK_TOOL_NAMES = frozenset(t["function"]["name"] for t in TASK_TOOLS)
# Estes critérios são enviados em toda rodada por _messages_for_model.
# Ao adicionar ferramentas, acrescente exemplos aqui se surgirem novos fluxos
# com várias etapas; mantenha a descrição de create_tasks coerente com a regra.
TASK_INSTRUCTIONS = """
PLANEJAMENTO E ACOMPANHAMENTO
Antes de agir, avalie se a solicitação exige várias etapas distintas.
Se exigir, você DEVE chamar create_tasks para registrar o plano. Essa ferramenta
serve para qualquer trabalho com várias etapas e não é exclusiva de /verify.
Se já houver um plano atual, siga-o em vez de criar outro.

Crie um plano quando houver várias entregas, dependências entre ações ou uma
investigação que exija acompanhar resultados de múltiplas ferramentas. Exemplos:
- "Encontre a planilha e analise os funcionários inativos": localizar o arquivo,
  examinar os dados relevantes e apresentar os achados.
- "Compare estes documentos": ler as partes pertinentes de cada um,
  conferir as diferenças e elaborar a comparação.
- "Pesquise o assunto e confira as fontes": buscar fontes, ler as páginas
  relevantes e responder com base nas evidências.
Para uma resposta direta ou consulta única, como saber a data atual ou listar
uma pasta, não crie etapas artificiais. Se a investigação crescer durante o
trabalho, registre um plano para as etapas restantes assim que perceber isso.

Descreva cada tarefa como uma ação concreta com resultado verificável.
Uma lista escrita em texto não registra tarefas nem atualiza o painel.
Depois de create_tasks, continue executando no mesmo turno, sem esperar outro
pedido do usuário. Use start_task ao iniciar uma etapa e chame as ferramentas
necessárias. Só use complete_task após conferir que o resultado foi obtido;
use fail_task em caso de erro e block_task quando faltar um dado, acesso ou decisão.
Não marque todas como concluídas apenas por ter criado o plano.
Não recrie o plano para apagar pendências. Prossiga nas etapas independentes.
Só apresente a resposta final quando não houver tarefas pending/in_progress.
Se houver tarefas failed/blocked, explique as limitações e os dados necessários.
""".strip()
