#include "terminal.h"

using namespace ftxui;

Component TerminalDisplay::initDisplay()
{
    return Renderer([]
    {
        return hbox({
            text("Bem-vindo!") | color(Color::Red),
            text(" Envie: ") | color(Color::White),
            text("/help") | color(Color::Magenta),
            text(" Para exibir todos os comandos") | color(Color::White),
        });
    });
}

Element TerminalDisplay::spinnerElement()
{
    return spinner(2, spinnerFrame.load()) | bold | color(Color::Yellow2);
}
