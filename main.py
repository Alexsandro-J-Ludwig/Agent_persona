"""Entrada do chat no terminal. Execute com `python main.py`.

Mantenha aqui apenas inicialização e interação com o usuário. A lógica no
pacote agent pode ser reutilizada futuramente por outras interfaces.
"""

import asyncio
import sys

from rich.console import Console

from agent import Agent, decide_think
from agent.config import create_client


async def main() -> None:
    """Cria a sessão e lê mensagens até sair, EOF ou Ctrl+C."""
    console = Console()
    agent = Agent(client=create_client(), console=console)
    console.print("[bold cyan]Olá! Como posso ajudar hoje?[/bold cyan]")
    console.print(
        "[dim]/think: com raciocínio | /fast: sem raciocínio | /sair: encerrar | /verify: validar tools[/dim]"
    )
    console.print(
        "[dim]\nAs respostas podem conter erros. Para informações importantes, confirme os dados na fonte original.[/dim]"
    )

    try:
        while True:
            user_prompt = input("\nUser: ").strip()
            if user_prompt.casefold() in {"/sair", "/exit"}:
                break
            if not user_prompt:
                continue

            # O roteador retorna o modo e o texto sem o comando de controle.
            think_mode, prompt = decide_think(user_prompt)
            await agent.run_agent(prompt, think_mode)
    except (EOFError, KeyboardInterrupt):
        console.print("\n[dim]Conversa encerrada.[/dim]")


# Importar este arquivo não deve abrir um chat automaticamente.
if __name__ == "__main__":
    # Pipes e logs continuam usando saída simples. --plain permite escolhê-la
    # manualmente; o terminal interativo usa conversa rolável e entrada fixa.
    if "--plain" in sys.argv or not sys.stdin.isatty() or not sys.stdout.isatty():
        asyncio.run(main())

    else:
        from agent.terminal import AgentTerminal

        AgentTerminal().run()
