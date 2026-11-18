#include "internalTools.h"

ToolResult InternalTools::listDirectory(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);

        const std::string pathStr =
            args.value("path", ".");

        // ====== LIMITES DA PÁGINA PARA PROTEGER O CONTEXTO ======
        constexpr std::size_t maxBytes = 8192;
        const int offset = args.value("offset", 0);
        const int limit = args.value("limit", 40);

        if (offset < 0 || limit < 1 || limit > 100)
            return {false, "", "Use offset >= 0 e limit entre 1 e 100."};

        std::error_code ec;

        const fs::path target = fs::weakly_canonical(
            fs::u8path(pathStr), ec);

        if (ec)
            return {false, "", ec.message()};

        if (!fs::exists(target, ec) || ec)
            return {false, "", "Diretório não encontrado."};

        if (!fs::is_directory(target, ec) || ec)
            return {false, "", "O caminho não é um diretório."};

        // ====== LISTAGEM RASA: SUBPASTAS SÃO CONSULTADAS SEPARADAMENTE ======
        fs::directory_iterator it(
            target,
            fs::directory_options::skip_permission_denied,
            ec);

        const fs::directory_iterator end;

        if (ec)
            return {false, "", ec.message()};

        std::string result;
        std::size_t index = 0;
        std::size_t returned = 0;
        bool truncated = false;
        bool incomplete = false;

        // ====== NOMES RELATIVOS, PAGINAÇÃO E LIMITE EM BYTES ======
        while (it != end)
        {
            if (index >= static_cast<std::size_t>(offset))
            {
                const auto utf8 = it->path().filename().u8string();
                std::string entry(reinterpret_cast<const char *>(utf8.data()), utf8.size());
                std::error_code typeError;
                if (it->is_directory(typeError))
                    entry += '/';
                entry += '\n';

                if (returned >= static_cast<std::size_t>(limit) ||
                    result.size() + entry.size() > maxBytes - 256)
                {
                    truncated = true;
                    break;
                }

                result += entry;
                ++returned;
            }

            ++index;
            it.increment(ec);
            if (ec)
            {
                incomplete = true;
                break;
            }
        }

        std::string header = "entries=" + std::to_string(returned) +
                             " truncated=" + (truncated ? "true" : "false");
        if (truncated)
            header += " next_offset=" + std::to_string(index);
        if (incomplete)
            header += " incomplete=true";
        header += "\n";

        return {true, header + result, ""};
    }
    catch (const std::exception &e)
    {
        return {false, "", e.what()};
    }
}
