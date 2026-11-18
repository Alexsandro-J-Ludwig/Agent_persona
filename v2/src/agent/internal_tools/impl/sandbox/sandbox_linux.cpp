
#ifndef _WIN32

#include "sandbox.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace Sandbox
{
    namespace
    {
        namespace fs = std::filesystem;
        using Clock = std::chrono::steady_clock;

        constexpr std::uintmax_t MAX_SOURCE_SIZE = 2 * 1024 * 1024;

        struct TempDirectory
        {
            fs::path path;

            ~TempDirectory()
            {
                if (!path.empty())
                {
                    std::error_code ec;
                    fs::remove_all(path, ec);
                }
            }
        };

        bool isInside(const fs::path &parent, const fs::path &child)
        {
            auto p = parent.begin();
            auto c = child.begin();

            for (; p != parent.end(); ++p, ++c)
            {
                if (c == child.end() || *p != *c)
                    return false;
            }

            return true;
        }

        bool prepareSource(
            const Config &config,
            fs::path &workspace,
            fs::path &source,
            std::string &error)
        {
            try
            {
                workspace = fs::canonical(config.workspace);

                fs::path candidate = config.sourceFile.is_absolute()
                                         ? config.sourceFile
                                         : workspace / config.sourceFile;

                candidate = candidate.lexically_normal();

                // Impede caminhos que tentem sair da workspace.
                if (!isInside(workspace, candidate))
                {
                    error = "O arquivo está fora da workspace.";
                    return false;
                }

                // Rejeita symlinks em qualquer componente do caminho.
                fs::path current = workspace;

                auto relative = candidate.lexically_relative(workspace);

                for (const auto &component : relative)
                {
                    current /= component;

                    const auto status = fs::symlink_status(current);

                    if (fs::is_symlink(status))
                    {
                        error = "Symlinks não são permitidos no caminho do arquivo.";
                        return false;
                    }
                }

                source = fs::canonical(candidate);

                if (!isInside(workspace, source))
                {
                    error = "O arquivo aponta para fora da workspace.";
                    return false;
                }

                if (!fs::is_regular_file(source))
                {
                    error = "O caminho informado não é um arquivo regular.";
                    return false;
                }

                const auto size = fs::file_size(source);

                if (size > MAX_SOURCE_SIZE)
                {
                    error = "O arquivo excede o limite de 2 MB.";
                    return false;
                }

                return true;
            }
            catch (const std::exception &e)
            {
                error = std::string("Falha ao validar o arquivo: ") + e.what();
                return false;
            }
        }

        bool createTempDirectory(TempDirectory &temp, std::string &error)
        {
            std::string pattern = "/tmp/incube-sandbox-XXXXXX";
            std::vector<char> buffer(pattern.begin(), pattern.end());
            buffer.push_back('\0');

            char *result = mkdtemp(buffer.data());

            if (!result)
            {
                error = std::string("Não foi possível criar diretório temporário: ") + std::strerror(errno);
                return false;
            }

            temp.path = result;

            if (chmod(temp.path.c_str(), 0700) != 0)
            {
                error = std::string("Não foi possível proteger o diretório temporário: ") + std::strerror(errno);
                return false;
            }

            return true;
        }

        bool copySource(
            const fs::path &source,
            const fs::path &destination,
            std::string &error)
        {
            try
            {
                fs::copy_file(
                    source,
                    destination,
                    fs::copy_options::overwrite_existing);

                fs::permissions(
                    destination,
                    fs::perms::owner_read,
                    fs::perm_options::replace);

                return true;
            }
            catch (const std::exception &e)
            {
                error = std::string("Falha ao copiar o código: ") + e.what();
                return false;
            }
        }

        struct StageResult
        {
            bool started = false;
            bool timedOut = false;
            bool outputTruncated = false;
            int exitCode = -1;
            std::string output;
            std::string error;
        };

        StageResult runStage(
            const Config &config,
            const fs::path &temp,
            const std::vector<std::string> &command,
            Clock::time_point deadline,
            std::size_t remainingOutput)
        {
            StageResult result;

            if (command.empty())
            {
                result.error = "Comando vazio.";
                return result;
            }

            int pipefd[2];

            if (pipe2(pipefd, O_CLOEXEC) != 0)
            {
                result.error = std::string("Falha ao criar pipe: ") + std::strerror(errno);
                return result;
            }

            const pid_t pid = fork();

            if (pid < 0)
            {
                result.error = std::string("Falha ao criar processo: ") + std::strerror(errno);

                close(pipefd[0]);
                close(pipefd[1]);
                return result;
            }

            if (pid == 0)
            {
                close(pipefd[0]);

                setpgid(0, 0);

                dup2(pipefd[1], STDOUT_FILENO);
                dup2(pipefd[1], STDERR_FILENO);

                close(pipefd[1]);

                std::vector<std::string> arguments;

                arguments.emplace_back("bwrap");
                arguments.emplace_back("--die-with-parent");
                arguments.emplace_back("--new-session");
                arguments.emplace_back("--unshare-all");

                arguments.emplace_back("--ro-bind");
                arguments.emplace_back("/usr");
                arguments.emplace_back("/usr");

                arguments.emplace_back("--ro-bind-try");
                arguments.emplace_back("/bin");
                arguments.emplace_back("/bin");

                arguments.emplace_back("--ro-bind-try");
                arguments.emplace_back("/lib");
                arguments.emplace_back("/lib");

                arguments.emplace_back("--ro-bind-try");
                arguments.emplace_back("/lib64");
                arguments.emplace_back("/lib64");

                arguments.emplace_back("--dir");
                arguments.emplace_back("/etc");

                arguments.emplace_back("--ro-bind-try");
                arguments.emplace_back("/etc/ld.so.cache");
                arguments.emplace_back("/etc/ld.so.cache");

                arguments.emplace_back("--dir");
                arguments.emplace_back("/etc/alternatives");

                arguments.emplace_back("--ro-bind-try");
                arguments.emplace_back("/etc/alternatives");
                arguments.emplace_back("/etc/alternatives");

                arguments.emplace_back("--proc");
                arguments.emplace_back("/proc");

                arguments.emplace_back("--dev");
                arguments.emplace_back("/dev");

                arguments.emplace_back("--tmpfs");
                arguments.emplace_back("/tmp");

                arguments.emplace_back("--dir");
                arguments.emplace_back("/work");

                arguments.emplace_back("--bind");
                arguments.emplace_back(temp.string());
                arguments.emplace_back("/work");

                arguments.emplace_back("--chdir");
                arguments.emplace_back("/work");

                arguments.emplace_back("--clearenv");

                arguments.emplace_back("--setenv");
                arguments.emplace_back("PATH");
                arguments.emplace_back("/usr/bin:/bin");

                arguments.emplace_back("--setenv");
                arguments.emplace_back("HOME");
                arguments.emplace_back("/tmp");

                arguments.emplace_back("--");

                // prlimit aplica limites de memória e CPU ao processo
                // executado e aos processos que ele iniciar.
                arguments.emplace_back("prlimit");

                const auto memoryBytes =
                    config.memoryLimitMB * 1024ULL * 1024ULL;

                const auto cpuSeconds = std::max<std::int64_t>(
                    1,
                    std::chrono::duration_cast<std::chrono::seconds>(
                        config.timeout)
                        .count());

                arguments.emplace_back(
                    "--as=" + std::to_string(memoryBytes));

                arguments.emplace_back(
                    "--cpu=" + std::to_string(cpuSeconds));

                arguments.emplace_back("--");

                for (const auto &arg : command)
                    arguments.emplace_back(arg);

                std::vector<char *> argv;
                argv.reserve(arguments.size() + 1);

                for (auto &arg : arguments)
                    argv.push_back(arg.data());

                argv.push_back(nullptr);

                execvp("bwrap", argv.data());

                const char *message = "Falha ao iniciar Bubblewrap ou prlimit.\n";
                write(STDERR_FILENO, message, std::strlen(message));

                _exit(127);
            }

            result.started = true;

            close(pipefd[1]);
            setpgid(pid, pid);

            const int flags = fcntl(pipefd[0], F_GETFL, 0);

            if (flags >= 0)
                fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

            bool pipeOpen = true;
            bool processExited = false;
            int status = 0;

            std::array<char, 4096> buffer{};

            while (pipeOpen || !processExited)
            {
                if (Clock::now() >= deadline && !processExited)
                {
                    result.timedOut = true;
                    kill(-pid, SIGKILL);
                    kill(pid, SIGKILL);
                }

                if (!processExited)
                {
                    const pid_t waited = waitpid(pid, &status, WNOHANG);

                    if (waited == pid)
                        processExited = true;
                    else if (waited < 0 && errno != EINTR)
                    {
                        processExited = true;
                        result.error = "Falha ao aguardar o processo.";
                    }
                }

                if (!pipeOpen)
                {
                    if (processExited)
                        break;

                    poll(nullptr, 0, 10);
                    continue;
                }

                pollfd pfd{};
                pfd.fd = pipefd[0];
                pfd.events = POLLIN | POLLHUP;

                poll(&pfd, 1, 20);

                while (true)
                {
                    const ssize_t count = read(
                        pipefd[0],
                        buffer.data(),
                        buffer.size());

                    if (count > 0)
                    {
                        const std::size_t amount =
                            static_cast<std::size_t>(count);

                        const std::size_t available =
                            remainingOutput > result.output.size()
                                ? remainingOutput - result.output.size()
                                : 0;

                        const std::size_t toStore =
                            std::min(amount, available);

                        result.output.append(buffer.data(), toStore);

                        if (toStore < amount)
                            result.outputTruncated = true;

                        continue;
                    }

                    if (count == 0)
                    {
                        pipeOpen = false;
                        close(pipefd[0]);
                        break;
                    }

                    if (errno == EINTR)
                        continue;

                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;

                    pipeOpen = false;
                    close(pipefd[0]);
                    break;
                }
            }

            if (!processExited)
                waitpid(pid, &status, 0);

            if (WIFEXITED(status))
                result.exitCode = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                result.exitCode = 128 + WTERMSIG(status);

            return result;
        }

        bool supportedExtension(const std::string &extension)
        {
            return extension == ".py" || extension == ".c" || extension == ".cpp" || extension == ".rs";
        }
    }

    Result run(const Config &config)
    {
        Result result;

        if (config.timeout.count() <= 0 ||
            config.memoryLimitMB == 0 ||
            config.maxOutputBytes == 0)
        {
            result.error = "Configuração de sandbox inválida.";
            return result;
        }

        if (config.memoryLimitMB >
            std::numeric_limits<std::uint64_t>::max() / (1024ULL * 1024ULL))
        {
            result.error = "Limite de memória inválido.";
            return result;
        }

        fs::path workspace;
        fs::path source;

        if (!prepareSource(config, workspace, source, result.error))
            return result;

        const std::string extension = source.extension().string();

        if (!supportedExtension(extension))
        {
            result.error =
                "Extensão não executável. Suportadas: .py, .c, .cpp e .rs.";
            return result;
        }

        TempDirectory temp;

        if (!createTempDirectory(temp, result.error))
            return result;

        const fs::path sandboxSource = temp.path / ("source" + extension);

        if (!copySource(source, sandboxSource, result.error))
            return result;

        const auto deadline = Clock::now() + config.timeout;

        std::vector<std::string> compileCommand;
        std::vector<std::string> executeCommand;

        if (extension == ".py")
        {
            executeCommand = {
                "python3",
                "-I",
                "-B",
                "/work/source.py"};
        }
        else if (extension == ".c")
        {
            compileCommand = {
                "gcc",
                "-O2",
                "-std=c17",
                "-Wall",
                "-Wextra",
                "/work/source.c",
                "-o",
                "/work/program"};

            executeCommand = {"/work/program"};
        }
        else if (extension == ".cpp")
        {
            compileCommand = {
                "g++",
                "-O2",
                "-std=c++20",
                "-Wall",
                "-Wextra",
                "/work/source.cpp",
                "-o",
                "/work/program"};

            executeCommand = {"/work/program"};
        }
        else if (extension == ".rs")
        {
            compileCommand = {
                "rustc",
                "-O",
                "/work/source.rs",
                "-o",
                "/work/program"};

            executeCommand = {"/work/program"};
        }

        if (!compileCommand.empty())
        {
            auto compiled = runStage(
                config,
                temp.path,
                compileCommand,
                deadline,
                config.maxOutputBytes);

            result.output += compiled.output;
            result.outputTruncated |= compiled.outputTruncated;

            if (compiled.timedOut)
            {
                result.timedOut = true;
                result.error = "Tempo limite excedido durante a compilação.";
                return result;
            }

            if (!compiled.error.empty())
            {
                result.error = compiled.error;
                return result;
            }

            if (compiled.exitCode != 0)
            {
                result.exitCode = compiled.exitCode;
                result.error = "Falha na compilação.";
                return result;
            }
        }

        const std::size_t remaining =
            result.output.size() < config.maxOutputBytes
                ? config.maxOutputBytes - result.output.size()
                : 0;

        if (Clock::now() >= deadline)
        {
            result.timedOut = true;
            result.error = "Tempo limite total excedido.";
            return result;
        }

        auto executed = runStage(
            config,
            temp.path,
            executeCommand,
            deadline,
            remaining);

        result.output += executed.output;
        result.outputTruncated |= executed.outputTruncated;
        result.exitCode = executed.exitCode;
        result.timedOut = executed.timedOut;

        if (executed.timedOut)
            result.error = "Tempo limite excedido durante a execução.";
        else if (!executed.error.empty())
            result.error = executed.error;
        else
            result.success = executed.exitCode == 0;

        return result;
    }

} // namespace Sandbox

#endif
