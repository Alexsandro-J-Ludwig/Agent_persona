#ifndef TERMINAL_H
#define TERMINAL_H

#include <ftxui/ftxui.hpp>
#include <string>

using namespace ftxui;

class Terminal
{
public:
    void displayChat(const std::string &role, const std::string &message);
};

#endif