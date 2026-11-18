#include "internalTools.h"

ToolResult InternalTools::compressor(
    const std::string &arguments)
{
    Ollama::json history;

    try
    {
        history = Ollama::json::parse(arguments);

        if (!history.is_array())
        {
            return {
                false,
                "",
                "O histórico precisa ser um array JSON."};
        }
    }
    catch (const Ollama::json::exception &e)
    {
        return {
            false,
            "",
            e.what()};
    }

    std::string system = std::format(R"(
You compress conversation history into memory for future turns.
The original messages will no longer be available.
Preserve information needed to continue accurately.

Rules:
Preserve every explicit fact that remains valid, even if mentioned only once.
Frequency does not determine importance.

An update changes only the explicitly named fields.
All other previously confirmed fields remain unchanged.

For each project or entity, retain its known attributes:
names, roles, language, versions, identifiers, dates and limits.

Before outputting, check the source history against the memory.
Restore any omitted valid fact. Do not output this check.

Place confirmed facts under ESTADO ATUAL.
Use INCERTEZAS only for explicitly uncertain information or requested
information that was never supplied.

Omit empty sections. Do not add entries saying that no tasks exist
unless their absence was explicitly established.

ESTADO ATUAL
ALTERAÇÕES E DECISÕES ANTERIORES
TAREFAS ATIVAS E PENDÊNCIAS
TAREFAS CONCLUÍDAS OU CANCELADAS
INCERTEZAS E INFORMAÇÕES NÃO FORNECIDAS

<history>
{}
</history>
)",
                                     history.dump(2, ' ', false, Ollama::json::error_handler_t::replace));

    constexpr int maxAttempts = 3;

    std::string lastError;

    for (int attempt = 0; attempt < maxAttempts; attempt++)
    {
        try
        {
            auto response = ollama.generate(
                getModel(ModelRole::Agent),
                system,
                false);

            std::string summary =
                response.at("response").get<std::string>();

            if (summary.empty())
            {
                lastError = "O compressor retornou uma resposta vazia.";
                continue;
            }

            return {
                true,
                summary,
                ""};
        }
        catch (const std::exception &e)
        {
            lastError = e.what();
        }
    }

    return {
        false,
        "",
        lastError};
}
