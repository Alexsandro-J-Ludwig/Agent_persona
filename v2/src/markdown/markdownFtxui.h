#ifndef MARKDOWN_FTXUI_H
#define MARKDOWN_FTXUI_H

#include "markdownRenderer.h"
#include <ftxui/dom/elements.hpp>

// Camada VISUAL do terminal: converte AST Markdown em elementos FTXUI.
// Nunca modifica o parser, e não recebe chunks diretamente.
namespace terminal_markdown {
ftxui::Element render(const MarkdownNode& document);
}

#endif
