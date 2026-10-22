#include "./src/chat/chat.h"
#include <filesystem>
#include <iostream>

int main()
{
    std::ofstream log_file("logs.log");
    std::streambuf *old_cerr = std::cerr.rdbuf(log_file.rdbuf());

    Chat chat;
    chat.run();

    return 0;
}
