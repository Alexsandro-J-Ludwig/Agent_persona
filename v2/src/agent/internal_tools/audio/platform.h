
#pragma once

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/types.h>
#endif

namespace platform
{
        namespace fs = std::filesystem;

        struct PipeProcess
        {
                FILE *stream = nullptr;

#ifdef _WIN32
                HANDLE process = nullptr;
                HANDLE job = nullptr;
#endif
        };

#ifdef _WIN32

        inline std::wstring toWide(const std::string &value)
        {
                if (value.empty())
                        return {};

                const int size = MultiByteToWideChar(
                    CP_UTF8, 0, value.c_str(), -1, nullptr, 0);

                if (size <= 0)
                        return {};

                std::wstring result(size, L'\0');

                MultiByteToWideChar(
                    CP_UTF8, 0, value.c_str(), -1,
                    result.data(), size);

                result.resize(size - 1);
                return result;
        }

        inline void releasePipe(PipeProcess &pipe)
        {
                if (pipe.stream)
                {
                        std::fclose(pipe.stream);
                        pipe.stream = nullptr;
                }

                if (pipe.process)
                {
                        CloseHandle(pipe.process);
                        pipe.process = nullptr;
                }

                if (pipe.job)
                {
                        CloseHandle(pipe.job);
                        pipe.job = nullptr;
                }
        }

#endif

        inline PipeProcess openPipeWrite(const std::string &cmd)
        {
                PipeProcess pipe;

#ifdef _WIN32

                SECURITY_ATTRIBUTES security{};
                security.nLength = sizeof(security);
                security.bInheritHandle = TRUE;

                HANDLE readPipe = nullptr;
                HANDLE writePipe = nullptr;

                if (!CreatePipe(
                        &readPipe,
                        &writePipe,
                        &security,
                        0))
                {
                        return pipe;
                }

                SetHandleInformation(
                    writePipe,
                    HANDLE_FLAG_INHERIT,
                    0);

                wchar_t systemDirectory[MAX_PATH];

                GetSystemDirectoryW(systemDirectory, MAX_PATH);

                std::wstring cmdPath =
                    std::wstring(systemDirectory) + L"\\cmd.exe";

                std::wstring wideCmd = toWide(cmd);

                if (wideCmd.empty())
                {
                        CloseHandle(readPipe);
                        CloseHandle(writePipe);
                        return pipe;
                }

                // cmd.exe /D /S /C "comando"
                std::wstring commandLine =
                    L"\"" + cmdPath +
                    L"\" /D /S /C \"\"" +
                    wideCmd + L"\"";

                STARTUPINFOW startup{};
                startup.cb = sizeof(startup);
                startup.dwFlags = STARTF_USESTDHANDLES;

                startup.hStdInput = readPipe;
                startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
                startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);

                PROCESS_INFORMATION processInfo{};

                pipe.job = CreateJobObjectW(nullptr, nullptr);

                if (!pipe.job)
                {
                        CloseHandle(readPipe);
                        CloseHandle(writePipe);
                        return pipe;
                }

                JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo{};

                jobInfo.BasicLimitInformation.LimitFlags =
                    JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

                if (!SetInformationJobObject(
                        pipe.job,
                        JobObjectExtendedLimitInformation,
                        &jobInfo,
                        sizeof(jobInfo)))
                {
                        CloseHandle(readPipe);
                        CloseHandle(writePipe);
                        CloseHandle(pipe.job);
                        pipe.job = nullptr;
                        return pipe;
                }

                if (!CreateProcessW(
                        cmdPath.c_str(),
                        commandLine.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        CREATE_SUSPENDED | CREATE_NO_WINDOW,
                        nullptr,
                        nullptr,
                        &startup,
                        &processInfo))
                {
                        CloseHandle(readPipe);
                        CloseHandle(writePipe);
                        CloseHandle(pipe.job);
                        pipe.job = nullptr;
                        return pipe;
                }

                CloseHandle(readPipe);

                pipe.process = processInfo.hProcess;

                if (!AssignProcessToJobObject(
                        pipe.job,
                        processInfo.hProcess))
                {
                        TerminateProcess(processInfo.hProcess, 1);
                        WaitForSingleObject(processInfo.hProcess, INFINITE);

                        CloseHandle(processInfo.hThread);
                        CloseHandle(writePipe);

                        CloseHandle(pipe.process);
                        CloseHandle(pipe.job);

                        pipe.process = nullptr;
                        pipe.job = nullptr;

                        return pipe;
                }

                ResumeThread(processInfo.hThread);
                CloseHandle(processInfo.hThread);

                // Converte HANDLE em FILE* para manter fwrite/fflush.
                const int fd = _open_osfhandle(
                    reinterpret_cast<intptr_t>(writePipe),
                    _O_WRONLY | _O_BINARY);

                if (fd == -1)
                {
                        TerminateJobObject(pipe.job, 1);
                        WaitForSingleObject(pipe.process, INFINITE);
                        CloseHandle(writePipe);
                        releasePipe(pipe);
                        return {};
                }

                pipe.stream = _fdopen(fd, "w");

                if (!pipe.stream)
                {
                        _close(fd);
                        TerminateJobObject(pipe.job, 1);
                        WaitForSingleObject(pipe.process, INFINITE);
                        releasePipe(pipe);
                        return {};
                }

#else

                pipe.stream = popen(cmd.c_str(), "w");

#endif

                return pipe;
        }

        inline void closePipe(PipeProcess &pipe)
        {
#ifdef _WIN32

                // Fecha a entrada do Piper para permitir encerramento normal.
                if (pipe.stream)
                {
                        std::fclose(pipe.stream);
                        pipe.stream = nullptr;
                }

                if (pipe.process)
                {
                        WaitForSingleObject(pipe.process, INFINITE);
                }

                releasePipe(pipe);

#else

                if (pipe.stream)
                {
                        pclose(pipe.stream);
                        pipe.stream = nullptr;
                }

#endif
        }

        inline void stopPipe(PipeProcess &pipe)
        {
#ifdef _WIN32

                // Encerra todos os processos pertencentes ao Job.
                if (pipe.job)
                {
                        TerminateJobObject(pipe.job, 1);
                }

                if (pipe.process)
                {
                        WaitForSingleObject(pipe.process, INFINITE);
                }

                releasePipe(pipe);

#else

                closePipe(pipe);

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

                const fs::path ff = fs::absolute(
                    fs::path("libs") / "ffplay" / "windows" / "ffplay.exe");

                return quote(ff) +
                       " -nodisp -loglevel error -fflags nobuffer -probesize 32 -autoexit"
                       " -f s16le -ar " +
                       r +
                       " -ch_layout mono -i -";

#else

                return "aplay -q -r " + r + " -f S16_LE -c 1 -t raw -";

#endif
        }

}