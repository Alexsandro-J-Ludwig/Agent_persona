#include "internalTools.h"

ToolResult InternalTools::listDirectory(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);

        const std::string pathStr =
            args.value("path", ".");

        std::error_code ec;

        const fs::path target = fs::weakly_canonical(
            fs::u8path(pathStr), ec);

        if (ec)
            return {false, "", ec.message()};

        if (!fs::exists(target, ec) || ec)
            return {false, "", "Diretório não encontrado."};

        if (!fs::is_directory(target, ec) || ec)
            return {false, "", "O caminho não é um diretório."};

        std::string result;

        auto appendPath = [&result](const fs::path &path)
        {
            const auto utf8 = path.u8string();

            result.append(
                reinterpret_cast<const char *>(utf8.data()),
                utf8.size());

            result += '\n';
        };

        appendPath(target);

        fs::recursive_directory_iterator it(
            target,
            fs::directory_options::skip_permission_denied,
            ec);

        const fs::recursive_directory_iterator end;

        if (ec)
            return {false, "", ec.message()};

        while (it != end)
        {
            appendPath(it->path());

            it.increment(ec);

            if (ec)
                ec.clear();
        }

        return {true, result, ""};
    }
    catch (const std::exception &e)
    {
        return {false, "", e.what()};
    }
}