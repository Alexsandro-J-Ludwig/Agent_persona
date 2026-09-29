"""Diagnóstico reproduzível de /verify, independente das escolhas do modelo.

Cada schema/registro vira uma tarefa. Casos locais usam dados de teste; integrações
sem entrada apropriada ficam bloqueadas, nunca contabilizadas como testadas.
Adicione um caso em _local_cases ao criar uma ferramenta testável sem dados reais.
"""
import inspect
from pathlib import Path
from tempfile import TemporaryDirectory

from .tasks import TASK_TOOLS, TaskManager


def error_message(result) -> str | None:
    """Convenções de erro já utilizadas pelas ferramentas deste projeto."""
    if isinstance(result, dict):
        value = result.get("erro") or result.get("error")
        if value:
            return str(value)
        if result.get("status") in ("error", "failed"):
            return str(result)
    if isinstance(result, str) and result.casefold().startswith(("erro", "tool desconhecida", "formato não suportado")):
        return result
    return None


def _local_cases(folder: Path):
    file = folder / "codex_verify.txt"
    file.write_text("MARCADOR_VERIFY", encoding="utf-8")
    return {
        "today": ({}, lambda r: isinstance(r, dict) and all(k in r for k in ("data", "hora", "dia_semana"))),
        "date_previous": ({"data": "28/09/2026"}, lambda r: r.get("Dia da semana") == "segunda-feira"),
        "calc_data": ({"data": "28/09/2026", "dias": 1}, lambda r: r.get("data") == "29/09/2026"),
        "listar_diretorio": ({"caminho": str(folder)}, lambda r: any(i.get("nome") == file.name for i in r)),
        "pesquisar_arquivo": ({"termo": "codex_verify", "diretorio": str(folder)}, lambda r: any(i.get("nome") == file.name for i in r.get("resultados", []))),
        "ler_documento": ({"caminho": str(file)}, lambda r: r.get("conteudo") == "MARCADOR_VERIFY"),
        "buscar_documento": ({"caminho": str(file), "termo": "MARCADOR_VERIFY"}, lambda r: bool(r.get("trechos"))),
    }


def _check_internal(name):
    """Testa o controle de tarefas em outro manager, sem alterar o diagnóstico."""
    sample = TaskManager()
    sample.create(["Caso de teste"])
    if name == "create_tasks":
        assert sample.has_pending()
        return
    sample.start(1)
    if name == "start_task":
        assert sample.next().status == "in_progress"
        return
    method, expected = {
        "complete_task": (sample.complete, "completed"),
        "fail_task": (sample.fail, "failed"),
        "block_task": (sample.block, "blocked"),
    }[name]
    method(1, "Resultado controlado")
    assert sample.tasks[0].status == expected and not sample.has_pending()


def run_verification(manager, display, registry, schemas, invoke):
    """Percorre todos os itens mesmo quando um deles falha."""
    definitions = schemas + TASK_TOOLS
    internal_names = {t["function"]["name"] for t in TASK_TOOLS}
    names = list(dict.fromkeys([t["function"]["name"] for t in definitions] + list(registry)))
    manager.create([f"Verificar {name}" for name in names])
    display.refresh()
    with TemporaryDirectory(prefix="agent_verify_") as temp:
        cases = _local_cases(Path(temp))
        for task, name in zip(manager.tasks, names):
            manager.start(task.id)
            display.status(f"Verificando {name}...")
            try:
                matches = [d["function"] for d in definitions if d["function"]["name"] == name]
                if len(matches) != 1:
                    raise ValueError("Schema ausente ou duplicado.")
                if name in internal_names:
                    _check_internal(name)
                    manager.complete(task.id, "Controle interno validado em uma sessão isolada.")
                    continue
                func = registry.get(name)
                if not callable(func):
                    raise ValueError("Ferramenta sem função registrada.")
                parameters = matches[0]["parameters"]
                signature = inspect.signature(func)
                properties = parameters.get("properties", {})
                required = parameters.get("required", [])
                if not set(required) <= properties.keys():
                    raise ValueError("Parâmetro obrigatório não descrito no schema.")
                signature.bind(**{key: None for key in required})
                signature.bind(**{key: None for key in properties})
                if name not in cases:
                    reason = {
                        "consultar_colaborador": "Necessita matrícula real para testar a API.",
                        "consultar_colaboradores_ativos": "Necessita período de consulta informado pelo usuário.",
                        "search": "Teste da integração pendente: necessita uma consulta de pesquisa.",
                        "read_page": "Teste da integração pendente: necessita URL e acesso aos serviços configurados.",
                    }.get(name, "Ainda não há um caso de teste local definido para esta ferramenta.")
                    manager.block(task.id, "Registro e schema válidos. " + reason)
                    continue
                args, validate = cases[name]
                result = invoke(name, args)
                error = error_message(result)
                if error:
                    raise ValueError(error)
                if not validate(result):
                    raise ValueError("A chamada retornou um resultado diferente do esperado.")
                manager.complete(task.id, "Chamada local executada e resultado conferido.")
            except Exception as error:
                manager.fail(task.id, f"{type(error).__name__}: {error}")
            finally:
                display.refresh()
    return "Verificação finalizada. Bloqueada significa que a chamada real não foi testada.\n" + manager.context()
