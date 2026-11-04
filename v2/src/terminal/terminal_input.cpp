#include "terminal.h"

#include <utility>

using namespace ftxui;

Component TerminalDisplay::displayInput(SubmitCallback onSubmit)
{
    auto input = Input(&prompt, "Pergunte à IA...");

    auto renderer = Renderer(input, [input]
    {
        return hbox({
                   text("User: ") | color(Color::Blue),
                   input->Render() | flex,
               }) |
               border;
    });

    return CatchEvent(renderer, [this, onSubmit = std::move(onSubmit)](Event event)
    {
        if (event != Event::Return)
            return false;

        if (prompt.empty())
            return true;

        std::string submitted = prompt;
        prompt.clear();

        if (submitted == "/help")
        {
            showCommands = !showCommands;
            screen.PostEvent(Event::Custom);
            return true;
        }

        scrollPosition = 1.0f;
        onSubmit(submitted);
        return true;
    });
}

Element TerminalDisplay::commandView()
{
    auto table = Table({
        {"Commands", "Function"},
        {"/fast", "Force response speed"},
        {"/think", "Force thinking mode"},
        {"/models", "Exibe os modelos atuais"},
        {"/audio", "Ativa ou desativa o modo de audio"},
    });

    table.SelectAll().Separator(LIGHT);
    table.SelectRow(0).Decorate(bold);
    table.SelectRow(0).Decorate(color(Color::Cyan));
    table.SelectColumn(0).Decorate(color(Color::BlueLight));
    table.SelectColumn(1).Decorate(color(Color::GrayLight));

    return vbox({
               text("   COMANDOS DISPONÍVEIS") | bold | color(Color::Cyan),
               separator(),
               table.Render(),
           }) |
           borderRounded |
           bgcolor(Color::Palette256(235)) |
           size(WIDTH, GREATER_THAN, 48);
}
