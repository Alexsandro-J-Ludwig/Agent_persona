#include "markdownRenderer.h"

#include <md4c.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

    // Copia MD_ATTRIBUTE: MD4C não promete string terminada em \0.
    std::string attribute(const MD_ATTRIBUTE &a)
    {
        if (!a.text || a.size == 0)
            return {};
        return std::string(a.text, a.size);
    }

    // Decodificador básico de entidades HTML frequentes em textos de IA.
    // Entidades nomeadas pouco comuns ficam como texto literal.
    std::string utf8(std::uint32_t cp)
    {
        if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            cp = 0xFFFD;
        std::string out;
        if (cp < 0x80)
            out += static_cast<char>(cp);
        else if (cp < 0x800)
        {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        return out;
    }
    std::string entity(std::string_view e)
    {
        if (e == "&amp;")
            return "&";
        if (e == "&lt;")
            return "<";
        if (e == "&gt;")
            return ">";
        if (e == "&quot;")
            return "\"";
        if (e == "&apos;")
            return "'";
        if (e == "&nbsp;")
            return " ";
        if (e.size() > 3 && e[0] == '&' && e[1] == '#' && e.back() == ';')
        {
            const bool hex = e.size() > 4 && (e[2] == 'x' || e[2] == 'X');
            const size_t begin = hex ? 3 : 2;
            std::uint32_t value = 0;
            for (size_t i = begin; i + 1 < e.size(); ++i)
            {
                const char c = e[i];
                int digit = (c >= '0' && c <= '9') ? c - '0' : (hex && c >= 'a' && c <= 'f') ? c - 'a' + 10
                                                           : (hex && c >= 'A' && c <= 'F')   ? c - 'A' + 10
                                                                                             : -1;
                if (digit < 0 || digit >= (hex ? 16 : 10))
                    return std::string(e);
                if (value > 0x10FFFF)
                    return utf8(0xFFFD);
                value = value * (hex ? 16 : 10) + static_cast<unsigned>(digit);
            }
            return utf8(value);
        }
        return std::string(e);
    }

    // Os callbacks do MD4C entram/saem de nós: construímos a AST com
    // índices em vez de ponteiros (push_back pode realocar vetores).
    struct Builder
    {
        MarkdownNode root{MarkdownType::Document};
        std::vector<size_t> path;

        MarkdownNode &current()
        {
            MarkdownNode *node = &root;
            for (size_t i : path)
                node = &node->children[i];
            return *node;
        }

        void open(MarkdownNode node)
        {
            MarkdownNode &parent = current();
            parent.children.push_back(std::move(node));
            path.push_back(parent.children.size() - 1);
        }
        void close()
        {
            if (!path.empty())
                path.pop_back();
        }

        // Junta fragmentos adjacentes de texto para evitar milhares de nós.
        void appendText(const std::string &s)
        {
            MarkdownNode &parent = current();
            if (!parent.children.empty() && parent.children.back().type == MarkdownType::Text)
            {
                parent.children.back().text += s;
            }
            else
            {
                MarkdownNode t;
                t.type = MarkdownType::Text;
                t.text = s;
                parent.children.push_back(std::move(t));
            }
        }
    };

    int enterBlock(MD_BLOCKTYPE kind, void *info, void *data)
    {
        auto &b = *static_cast<Builder *>(data);
        if (kind == MD_BLOCK_DOC)
            return 0;
        MarkdownNode n;
        switch (kind)
        {
        case MD_BLOCK_P:
            n.type = MarkdownType::Paragraph;
            break;
        case MD_BLOCK_H:
            n.type = MarkdownType::Heading;
            n.headingLevel = static_cast<MD_BLOCK_H_DETAIL *>(info)->level;
            break;
        case MD_BLOCK_QUOTE:
            n.type = MarkdownType::Quote;
            break;
        case MD_BLOCK_UL:
            n.type = MarkdownType::UnorderedList;
            break;
        case MD_BLOCK_OL:
            n.type = MarkdownType::OrderedList;
            n.listStart = static_cast<MD_BLOCK_OL_DETAIL *>(info)->start;
            break;
        case MD_BLOCK_LI:
            n.type = MarkdownType::ListItem;
            if (info)
            {
                auto &d = *static_cast<MD_BLOCK_LI_DETAIL *>(info);
                n.task = d.is_task != 0;
                n.checked = n.task && (d.task_mark == 'x' || d.task_mark == 'X');
            }
            break;
        case MD_BLOCK_HR:
            n.type = MarkdownType::HorizontalRule;
            break;
        case MD_BLOCK_CODE:
            n.type = MarkdownType::CodeBlock;
            if (info)
                n.language = attribute(static_cast<MD_BLOCK_CODE_DETAIL *>(info)->lang);
            break;
        case MD_BLOCK_HTML:
            n.type = MarkdownType::RawHtml;
            break;
        case MD_BLOCK_TABLE:
            n.type = MarkdownType::Table;
            break;
        case MD_BLOCK_THEAD:
            n.type = MarkdownType::TableHead;
            break;
        case MD_BLOCK_TBODY:
            n.type = MarkdownType::TableBody;
            break;
        case MD_BLOCK_TR:
            n.type = MarkdownType::TableRow;
            break;
        case MD_BLOCK_TH:
            n.type = MarkdownType::TableHeaderCell;
            if (info)
                n.alignment = static_cast<MD_BLOCK_TD_DETAIL *>(info)->align;
            break;
        case MD_BLOCK_TD:
            n.type = MarkdownType::TableCell;
            if (info)
                n.alignment = static_cast<MD_BLOCK_TD_DETAIL *>(info)->align;
            break;
        default:
            n.type = MarkdownType::Paragraph;
            break;
        }
        b.open(std::move(n));
        return 0;
    }
    int leaveBlock(MD_BLOCKTYPE kind, void *, void *data)
    {
        if (kind != MD_BLOCK_DOC)
            static_cast<Builder *>(data)->close();
        return 0;
    }
    int enterSpan(MD_SPANTYPE kind, void *info, void *data)
    {
        auto &b = *static_cast<Builder *>(data);
        MarkdownNode n;
        switch (kind)
        {
        case MD_SPAN_EM:
            n.type = MarkdownType::Emphasis;
            break;
        case MD_SPAN_STRONG:
            n.type = MarkdownType::Strong;
            break;
        case MD_SPAN_DEL:
            n.type = MarkdownType::Strike;
            break;
        case MD_SPAN_CODE:
            n.type = MarkdownType::InlineCode;
            break;
        case MD_SPAN_LATEXMATH:
        case MD_SPAN_LATEXMATH_DISPLAY:
            n.type = MarkdownType::Math;
            break;
        case MD_SPAN_A:
            n.type = MarkdownType::Link;
            if (info)
            {
                auto &d = *static_cast<MD_SPAN_A_DETAIL *>(info);
                n.destination = attribute(d.href);
                n.title = attribute(d.title);
            }
            break;
        case MD_SPAN_IMG:
            n.type = MarkdownType::Image;
            if (info)
            {
                auto &d = *static_cast<MD_SPAN_IMG_DETAIL *>(info);
                n.destination = attribute(d.src);
                n.title = attribute(d.title);
            }
            break;
        default:
            n.type = MarkdownType::Text;
            break;
        }
        b.open(std::move(n));
        return 0;
    }
    int leaveSpan(MD_SPANTYPE, void *, void *data)
    {
        static_cast<Builder *>(data)->close();
        return 0;
    }
    int emitText(MD_TEXTTYPE kind, const MD_CHAR *raw, MD_SIZE size, void *data)
    {
        auto &b = *static_cast<Builder *>(data);
        if (kind == MD_TEXT_BR || kind == MD_TEXT_SOFTBR)
        {
            MarkdownNode n;
            n.type = (kind == MD_TEXT_BR) ? MarkdownType::HardBreak : MarkdownType::SoftBreak;
            b.current().children.push_back(std::move(n));
        }
        else if (kind == MD_TEXT_NULLCHAR)
        {
            b.appendText(utf8(0xFFFD));
        }
        else if (kind == MD_TEXT_ENTITY)
        {
            b.appendText(entity(std::string_view(raw, size)));
        }
        else
        {
            b.appendText(std::string(raw, size));
        }
        return 0;
    }
} // namespace

void MarkdownRenderer::append(const std::string &token)
{
    if (token.empty())
        return;
    source_ += token;
    dirty_ = true;
}

void MarkdownRenderer::rebuild() const
{
    Builder builder;
    if (!source_.empty())
    {
        if (source_.size() > std::numeric_limits<MD_SIZE>::max())
            throw std::length_error("Markdown excedeu tamanho aceito pelo MD4C");

        MD_PARSER parser{};
        parser.abi_version = 0;
        parser.flags = MD_DIALECT_GITHUB | MD_FLAG_LATEXMATHSPANS | MD_FLAG_NOHTML;
        parser.enter_block = &enterBlock;
        parser.leave_block = &leaveBlock;
        parser.enter_span = &enterSpan;
        parser.leave_span = &leaveSpan;
        parser.text = &emitText;

        if (md_parse(source_.data(), static_cast<MD_SIZE>(source_.size()),
                     &parser, &builder) != 0)
            throw std::runtime_error("Falha ao interpretar Markdown com MD4C");
    }
    ast_ = std::move(builder.root);
    dirty_ = false;
}

const MarkdownNode &MarkdownRenderer::document() const
{
    if (dirty_)
        rebuild();
    return ast_;
}

void MarkdownRenderer::finish()
{
    // Nenhuma limpeza: precisa guardar o documento para exibição e histórico.
    // A próxima chamada document() já entrega o estado final.
}
void MarkdownRenderer::clear()
{
    source_.clear();
    ast_ = MarkdownNode{MarkdownType::Document};
    dirty_ = false;
}
