#include "markdownRenderer.h"

/*
    Função ainda em processo de analise e cosntrução
*/
// ftxui::Element MarkdownRenderer::renderElements(const std::string &content, std::string element)
// {
//     ftxui::Elements elements;

//     size_t pos = 0;

//     while (pos < content.size())
//     {
//         size_t elementStart = content.find(element, pos);

//         if (elementStart == std::string::npos)
//         {
//             elements.push_back(
//                 ftxui::text(content.substr(pos)));
//             break;
//         }

//         if (elementStart > pos)
//         {
//             elements.push_back(
//                 ftxui::text(content.substr(pos, elementStart - pos)));
//         }

//         size_t elementEnd =
//             content.find(element, elementStart + 2);

//         if (elementEnd == std::string::npos)
//         {
//             elements.push_back(
//                 ftxui::text(content.substr(elementStart)));
//             break;
//         }

//         if (element == "#" || element == "##" || element == "###")
//         {
//             size_t contentStart = elementStart + 1;
//             size_t lineEnd = content.find('\n', contentStart);

//             std::string elementContent;

//             if (lineEnd == std::string::npos)
//             {
//                 elementContent = content.substr(contentStart);
//             }
//             else
//             {
//                 elementContent = content.substr(
//                     contentStart,
//                     lineEnd - contentStart);
//             }

//             if (element == "#")
//             {
//                 elements.push_back(
//                     ftxui::text(elementContent) | ftxui::bold | ftxui::underlined);
//             }
//             else if (element == "##")
//             {
//                 elements.push_back(
//                     ftxui::text(elementContent) | ftxui::bold);
//             }
//             else if (element == "###")
//             {
//                 elements.push_back(
//                     ftxui::text(elementContent) | ftxui::bold | ftxui::dim);
//             };
//         }
//         if (element == "**")
//         {
//         }
//     }
// }

ftxui::Element MarkdownRenderer::render(const std::string &markdown)
{
    Element element;

    size_t pos = 0;

    while (pos <)
}
