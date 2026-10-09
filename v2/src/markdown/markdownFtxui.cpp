#include "markdownFtxui.h"

#include <ftxui/dom/table.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace terminal_markdown {
namespace {
using namespace ftxui;

struct Style {
    bool strong = false;
    bool italic = false;
    bool strike = false;
    bool code = false;
    bool math = false;
    bool link = false;
    std::string url;
};

struct Fragment {
    std::string text;
    Style style;
    bool hardBreak = false;
};

// Texto puro (usado para blocos de código, rótulos e alternativas de imagem).
std::string plain(const MarkdownNode& node) {
    if (node.type == MarkdownType::HardBreak ||
        node.type == MarkdownType::SoftBreak) return "\n";
    std::string value = node.text;
    for (const auto& child : node.children) value += plain(child);
    return value;
}

// Constrói fragmentos formatados; estilos se acumulam, logo
// **negrito com *itálico*** funciona sem lógica especial.
void collect(const MarkdownNode& n, Style style, std::vector<Fragment>& out) {
    switch (n.type) {
        case MarkdownType::Strong: style.strong = true; break;
        case MarkdownType::Emphasis: style.italic = true; break;
        case MarkdownType::Strike: style.strike = true; break;
        case MarkdownType::InlineCode: style.code = true; break;
        case MarkdownType::Math:
            style.math = true;
            out.push_back({"$", style, false});
            for (const auto& child : n.children) collect(child, style, out);
            out.push_back({"$", style, false});
            return;
        case MarkdownType::Link:
            style.link = true;
            style.url = n.destination;
            break;
        case MarkdownType::Image:
            // Terminais tradicionais não exibem imagens; preserva alt e URL.
            out.push_back({"[imagem: " + plain(n) + "]" +
                          (n.destination.empty() ? "" : " (" + n.destination + ")"),
                           Style{false, false, false, false, false, true, n.destination}, false});
            return;
        case MarkdownType::HardBreak:
            out.push_back({"", style, true});
            return;
        case MarkdownType::SoftBreak:
            out.push_back({" ", style, false});
            return;
        default: break;
    }
    if (!n.text.empty()) out.push_back({n.text, style, false});
    for (const auto& child : n.children) collect(child, style, out);
}

Element styledText(const std::string& content, const Style& style) {
    Element node = text(content);
    if (style.strong) node = node | bold;
    if (style.italic) node = node | italic;
    if (style.strike) node = node | strikethrough;
    if (style.code) node = node | color(Color::YellowLight) | bgcolor(Color::Palette256(238));
    if (style.math) node = node | color(Color::MagentaLight);
    if (style.link) {
        node = node | color(Color::CyanLight) | underlined;
        // Evita protocolo arbitrário em sequências OSC 8 do terminal.
        if (style.url.starts_with("https://") ||
            style.url.starts_with("http://") ||
            style.url.starts_with("mailto:"))
            node = node | hyperlink(style.url);
    }
    return node;
}

// FTXUI não tem HTML layout; flexbox faz wrap dos fragmentos por palavra.
// O terminal desenha; o parser não sabe que isso existe.
Element inlineContent(const MarkdownNode& node) {
    std::vector<Fragment> fragments;
    collect(node, {}, fragments);

    Elements rows;
    Elements words;
    auto flushRow = [&]() {
        if (words.empty()) words.push_back(text(""));
        rows.push_back(flexbox(std::move(words)) | flex);
        words.clear();
    };

    for (const auto& frag : fragments) {
        if (frag.hardBreak) { flushRow(); continue; }
        const std::string& s = frag.text;
        size_t pos = 0;
        while (pos < s.size()) {
            const bool space = std::isspace(static_cast<unsigned char>(s[pos])) != 0;
            size_t end = pos + 1;
            while (end < s.size() &&
                   (std::isspace(static_cast<unsigned char>(s[end])) != 0) == space)
                ++end;
            if (space) {
                // Preserva separação entre palavras, mas normaliza whitespace.
                words.push_back(styledText(" ", frag.style));
            } else {
                words.push_back(styledText(s.substr(pos, end - pos), frag.style));
            }
            pos = end;
        }
    }
    flushRow();
    return vbox(std::move(rows)) | flex;
}

Element renderBlock(const MarkdownNode& node);

Element childrenVertical(const MarkdownNode& node, bool addSpacing) {
    Elements rows;
    for (const auto& child : node.children) {
        rows.push_back(renderBlock(child));
        if (addSpacing) rows.push_back(text(""));
    }
    if (addSpacing && !rows.empty()) rows.pop_back();
    return vbox(std::move(rows)) | flex;
}

Element renderList(const MarkdownNode& list) {
    Elements items;
    unsigned number = list.listStart;
    for (const auto& item : list.children) {
        if (item.type != MarkdownType::ListItem) continue;
        std::string marker = (list.type == MarkdownType::OrderedList)
            ? std::to_string(number++) + ". " : "• ";
        if (item.task) marker = item.checked ? "[x] " : "[ ] ";
        items.push_back(hbox({
            text(marker) | color(Color::BlueLight),
            childrenVertical(item, false) | flex
        }));
    }
    return vbox(std::move(items)) | flex;
}

Element renderCodeBlock(const MarkdownNode& node) {
    std::string code = plain(node);
    if (!code.empty() && code.back() == '\n') code.pop_back();

    Elements lines;
    if (!node.language.empty())
        lines.push_back(text("  " + node.language) | bold | color(Color::Cyan));

    size_t start = 0;
    while (start <= code.size()) {
        size_t end = code.find('\n', start);
        if (end == std::string::npos) end = code.size();
        // paragraph() permite quebra de linhas longas e evita cortar o código.
        lines.push_back(paragraph(code.substr(start, end-start)) | color(Color::GrayLight));
        if (end == code.size()) break;
        start = end + 1;
    }
    return vbox(std::move(lines))
        | bgcolor(Color::Palette256(235))
        | borderRounded
        | flex;
}

// Renderiza tabelas com a API nativa de FTXUI; preserva alinhamento
// e links em texto. Estilos inline complexos ficam simplificados na tabela.
Element renderTable(const MarkdownNode& node) {
    std::vector<std::vector<std::string>> rows;
    bool hasHeader = false;
    const auto visitSection = [&](const MarkdownNode& section) {
        for (const auto& row : section.children) {
            if (row.type != MarkdownType::TableRow) continue;
            std::vector<std::string> cells;
            for (const auto& cell : row.children) {
                if (cell.type == MarkdownType::TableHeaderCell ||
                    cell.type == MarkdownType::TableCell)
                    cells.push_back(plain(cell));
            }
            if (!cells.empty()) rows.push_back(std::move(cells));
        }
    };
    for (const auto& section : node.children) {
        if (section.type == MarkdownType::TableHead) {
            hasHeader = true;
            visitSection(section);
        } else if (section.type == MarkdownType::TableBody) {
            visitSection(section);
        }
    }
    if (rows.empty()) return text("");
    auto table = Table(rows);
    table.SelectAll().Border(LIGHT);
    if (hasHeader) {
        table.SelectRow(0).Decorate(bold);
        table.SelectRow(0).Decorate(color(Color::Cyan));
    }
    return table.Render() | flex;
}

Element renderBlock(const MarkdownNode& node) {
    switch (node.type) {
        case MarkdownType::Document:
            return childrenVertical(node, true);
        case MarkdownType::Paragraph:
        case MarkdownType::TableHeaderCell:
        case MarkdownType::TableCell:
            return inlineContent(node);
        case MarkdownType::Heading: {
            Element heading = inlineContent(node) | bold;
            if (node.headingLevel == 1) {
                return vbox({heading | color(Color::CyanLight),
                             separator() | color(Color::Cyan)});
            }
            if (node.headingLevel == 2) return heading | color(Color::BlueLight);
            if (node.headingLevel == 3) return heading | color(Color::YellowLight);
            return heading | color(Color::GrayLight);
        }
        case MarkdownType::Quote:
            return hbox({text("▎ ") | color(Color::Cyan),
                         childrenVertical(node, false) | flex})
                | bgcolor(Color::Palette256(236));
        case MarkdownType::UnorderedList:
        case MarkdownType::OrderedList:
            return renderList(node);
        case MarkdownType::ListItem:
            return childrenVertical(node, false);
        case MarkdownType::CodeBlock:
            return renderCodeBlock(node);
        case MarkdownType::HorizontalRule:
            return separator() | color(Color::GrayDark);
        case MarkdownType::Table:
            return renderTable(node);
        case MarkdownType::RawHtml:
            return paragraph(plain(node)) | dim;
        default:
            return inlineContent(node);
    }
}
} // namespace

Element render(const MarkdownNode& document) {
    return renderBlock(document);
}
} // namespace terminal_markdown
