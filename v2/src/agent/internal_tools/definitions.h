#ifndef DEFINITION_H
#define DEFINITION_H

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace Definitions
{
    inline fs::path get_workspace_path()
    {
        // Permite sobrescrever por variável de ambiente
        if (const char *custom = std::getenv("AGENT_WORKSPACE"))
            return fs::path(custom);

#ifdef _WIN32
        const char *base = std::getenv("LOCALAPPDATA");
        if (!base)
            throw std::runtime_error("LOCALAPPDATA não definida");
        return fs::path(base) / "agent" / "workspace";
#else
        if (const char *xdg = std::getenv("XDG_DATA_HOME"))
            return fs::path(xdg) / "agent" / "workspace";

        const char *home = std::getenv("HOME");
        if (!home)
            throw std::runtime_error("HOME não definida");
        return fs::path(home) / ".local" / "share" / "agent" / "workspace";
#endif
    }

    inline fs::path ensure_workspace()
    {
        fs::path path = get_workspace_path();
        fs::create_directories(path); // não dá erro se já existir
        return path;
    }
}

#endif