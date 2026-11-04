// platform.h
#pragma once
#include <cstdio>
#include <filesystem>
#include <string>

namespace platform
{
        namespace fs = std::filesystem;

        inline FILE *openPipeWrite(const std::string &cmd)
        {
#ifdef _WIN32
                return _popen(("\"" + cmd + "\"").c_str(), "w");
#else
                return popen(cmd.c_str(), "w");
#endif
        }

        inline void closePipe(FILE *f)
        {
#ifdef _WIN32
                _pclose(f);
#else
                pclose(f);
#endif
        }

        inline fs::path piperExecutable()
        {
#ifdef _WIN32
                return fs::path("libs") / "piper" / "windows" / "piper.exe";
#else
                return fs::path("libs") / "piper" / "linux" / "piper";
#endif
        }

        inline std::string quote(const fs::path &p)
        {
                return "\"" + fs::path(p).make_preferred().string() + "\"";
        }

        inline std::string playerCommand(int rate)
        {
                const std::string r = std::to_string(rate);
#ifdef _WIN32
                const fs::path ff = fs::absolute(fs::path("libs") / "ffplay" / "windows" / "ffplay.exe");
                return quote(ff) +
                       " -nodisp -loglevel error -fflags nobuffer -probesize 32 -autoexit"
                       " -f s16le -ar " +
                       r + " -ch_layout mono -i -";
#else
                return "aplay -q -r " + r + " -f S16_LE -c 1 -t raw -";
#endif
        }
} // namespace platform
