#ifndef AGENTCONFIG_H
#define AGENTCONFIG_H

#include <cstddef>

namespace Config
{
    struct Properties
    {
        std::size_t max_rounds = 20;
        std::size_t max_msg = 32;
        std::size_t max_bytes = 50000;

        const char *system_prompt =
            R"(Você é um assistente de IA local com acesso às ferramentas fornecidas.
            Responda com frases curtas sem texto discursivo. Fale frases curtas e mais explicativas. Sem enrolar)";
    };
}

#endif