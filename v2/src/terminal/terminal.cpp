#include "terminal.h"

void displayChat(
    const std::string &role,
    const std::string &message)
{
    Component chat = Renderer([&]
                              { return hbox({
                                    text(role + ": ") | bold,
                                    separator(),
                                    paragraph(message) | flex,
                                }); });

    Component layout = Container::Vertical({chat});

    Component renderer = Renderer(layout, [&]
                                  { return vbox({
                                               chat->Render() | flex,
                                               separator(),
                                           }) |
                                           border; });
}