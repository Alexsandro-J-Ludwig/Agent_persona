#include "./src/chat/chat.h"
#include "src/agent/memory/memory.h"
#include "src/agent/internal_tools/internalTools.h"
#include <fstream>
#include <iostream>

int main()
{
    std::ofstream log_file("logs.log");
    std::streambuf *old_cerr = std::cerr.rdbuf(log_file.rdbuf());

    InternalTools tools;

    if (!Memory::instance().initialize(tools))
    {
        std::cerr.rdbuf(old_cerr);
        std::cerr << "Falha ao iniciar a memória. Veja logs.log\n";
        return 1;
    }

    Chat chat;
    chat.run();

    return 0;
}