#ifndef MARKDOWNRENDERER_H
#define MARKDOWNRENDERER_H

#include <ftxui/ftxui.hpp>

enum class ElementType
{
    Line,
    Delimited
};

struct MarkdownElement
{
    std::string marker;
    ElementType type;
};

class MarkdownRenderer
{
public:
    ftxui::Element render(const std::string &markdown);

private:
    // ftxui::Element renderHeading1(const std::string &content);
    // ftxui::Element renderHeading2(const std::string &conteWnt);
    // ftxui::Element renderHeading3(const std::string &content);
    // ftxui::Element renderHeading4(const std::string &content);
    // ftxui::Element renderParagraph(const std::string &content);
    // ftxui::Element renderCodeBlock(const std::string &content);
    // ftxui::Element renderList(const std::string &content);
    // ftxui::Element renderQuote(const std::string &content);
    // ftxui::Element renderInline(const std::string &content);
    ftxui::Element renderElements(const std::string &content, std::string element);
};

#endif