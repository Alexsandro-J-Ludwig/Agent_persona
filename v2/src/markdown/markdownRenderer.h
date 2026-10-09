#ifndef MARKDOWN_RENDERER_H
#define MARKDOWN_RENDERER_H

#include <string>
#include <vector>

// AST (árvore de sintaxe) neutra: SEM qualquer dependência do FTXUI.
// O parser identifica a estrutura; o terminal decide como desenhar.
enum class MarkdownType {
    Document, Paragraph, Heading, Quote, UnorderedList, OrderedList,
    ListItem, HorizontalRule, CodeBlock, Table, TableHead, TableBody,
    TableRow, TableHeaderCell, TableCell,
    Text, Emphasis, Strong, Strike, InlineCode, Link, Image,
    HardBreak, SoftBreak, Math, RawHtml
};

struct MarkdownNode {
    MarkdownType type = MarkdownType::Text;
    std::string text;             // Conteúdo de uma folha (texto bruto).
    std::string destination;      // URL de link/imagem.
    std::string title;            // Título opcional do link/imagem.
    std::string language;         // Linguagem de bloco de código, ex.: cpp.
    unsigned headingLevel = 0;   // 1..6 para Heading.
    unsigned listStart = 1;      // Início de lista numerada.
    bool task = false;           // Item de lista tem checkbox?
    bool checked = false;        // Checkbox marcado?
    int alignment = 0;           // 0 padrão, 1 esquerda, 2 centro, 3 direita.
    std::vector<MarkdownNode> children;

    MarkdownNode() = default;
    explicit MarkdownNode(MarkdownType kind) : type(kind) {}
};

class MarkdownRenderer {
public:
    // Recebe fragmentos do Ollama. NÃO cria ftxui::Element, NÃO desenha.
    void append(const std::string& token);

    // AST mais atual. Reprocessa o texto apenas se chegaram novos tokens.
    // MD4C não oferece estado incremental de parsing: parse de snapshot.
    const MarkdownNode& document() const;

    // Opcional ao concluir a resposta (mantém o documento para leitura).
    void finish();

    // Recomeça o parser para outra mensagem.
    void clear();

    const std::string& raw() const { return source_; }

private:
    std::string source_;         // Deve ser preservado para blocos multilinha.
    mutable MarkdownNode ast_{MarkdownType::Document};
    mutable bool dirty_ = true;
    void rebuild() const;
};

#endif
