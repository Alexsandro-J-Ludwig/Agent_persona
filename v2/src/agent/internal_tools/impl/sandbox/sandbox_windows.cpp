#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include "sandbox.h"

#include <windows.h>
#include <userenv.h>
#include <aclapi.h>
#include <objbase.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

namespace Sandbox
{
    namespace
    {
        namespace fs = std::filesystem;
        using Clock = std::chrono::steady_clock;

        constexpr std::uint64_t MAX_SOURCE_SIZE = 2ULL * 1024ULL * 1024ULL;
        constexpr std::uint64_t MAX_MEMORY_LIMIT_MB = 4096;
        constexpr std::size_t MAX_OUTPUT_LIMIT = 1024ULL * 1024ULL;

        struct Handle
        {
            HANDLE value = nullptr;

            Handle() = default;
            explicit Handle(HANDLE handle) : value(handle) {}

            ~Handle()
            {
                reset();
            }

            Handle(const Handle &) = delete;
            Handle &operator=(const Handle &) = delete;

            Handle(Handle &&other) noexcept : value(other.value)
            {
                other.value = nullptr;
            }

            Handle &operator=(Handle &&other) noexcept
            {
                if (this != &other)
                {
                    reset();
                    value = other.value;
                    other.value = nullptr;
                }
                return *this;
            }

            void reset(HANDLE handle = nullptr) noexcept
            {
                if (value && value != INVALID_HANDLE_VALUE)
                    CloseHandle(value);
                value = handle;
            }

            HANDLE get() const noexcept { return value; }
            explicit operator bool() const noexcept
            {
                return value != nullptr && value != INVALID_HANDLE_VALUE;
            }
        };

        std::string winError(const char *message, DWORD code = GetLastError())
        {
            return std::string(message) + " (Win32=" + std::to_string(code) + ")";
        }

        std::string hresultError(const char *message, HRESULT hr)
        {
            return std::string(message) + " (HRESULT=" +
                   std::to_string(static_cast<unsigned long>(hr)) + ")";
        }

        std::wstring lower(std::wstring value)
        {
            std::transform(
                value.begin(), value.end(), value.begin(),
                [](wchar_t ch)
                {
                    return static_cast<wchar_t>(std::towlower(ch));
                });
            return value;
        }

        bool isInside(const fs::path &parent, const fs::path &child)
        {
            const fs::path normalizedParent = parent.lexically_normal();
            const fs::path normalizedChild = child.lexically_normal();

            auto p = normalizedParent.begin();
            auto c = normalizedChild.begin();

            for (; p != normalizedParent.end(); ++p, ++c)
            {
                if (c == normalizedChild.end())
                    return false;

                if (lower(p->wstring()) != lower(c->wstring()))
                    return false;
            }

            return true;
        }

        std::wstring readEnvironmentVariable(const wchar_t *name)
        {
            const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
            if (required == 0)
                return {};

            std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1);
            const DWORD length = GetEnvironmentVariableW(
                name, buffer.data(), static_cast<DWORD>(buffer.size()));

            if (length == 0 || length >= buffer.size())
                return {};

            return std::wstring(buffer.data(), length);
        }

        std::wstring getWindowsDirectory()
        {
            UINT capacity = MAX_PATH;

            for (int attempt = 0; attempt < 3; ++attempt)
            {
                std::vector<wchar_t> buffer(static_cast<std::size_t>(capacity) + 1);
                const UINT length = GetWindowsDirectoryW(
                    buffer.data(), static_cast<UINT>(buffer.size()));

                if (length == 0)
                    throw std::runtime_error("Não foi possível obter o diretório do Windows.");

                if (length < buffer.size())
                    return std::wstring(buffer.data(), length);

                capacity = length + 1;
            }

            throw std::runtime_error("O caminho do diretório do Windows é excessivamente longo.");
        }

        std::wstring makeGuidString()
        {
            GUID guid{};
            if (FAILED(CoCreateGuid(&guid)))
                throw std::runtime_error("Falha ao gerar GUID para o sandbox.");

            wchar_t buffer[40]{};
            if (StringFromGUID2(guid, buffer, 40) == 0)
                throw std::runtime_error("Falha ao formatar GUID do sandbox.");

            std::wstring value(buffer);
            value.erase(
                std::remove_if(
                    value.begin(), value.end(),
                    [](wchar_t ch)
                    { return ch == L'{' || ch == L'}'; }),
                value.end());

            return value;
        }

        struct AppContainer
        {
            PSID sid = nullptr;
            std::wstring name;
            bool created = false;

            AppContainer() = default;
            AppContainer(const AppContainer &) = delete;
            AppContainer &operator=(const AppContainer &) = delete;

            ~AppContainer()
            {
                cleanupNoThrow();
            }

            bool create(std::string &error)
            {
                try
                {
                    // O nome só usa caracteres aceitos pela API; o GUID não contém
                    // as chaves retornadas por StringFromGUID2.
                    name = L"IncubeAgent-" + makeGuidString();

                    const HRESULT hr = CreateAppContainerProfile(
                        name.c_str(),
                        L"IncubeAgent Sandbox",
                        L"Isolated code execution",
                        nullptr,
                        0,
                        &sid);

                    if (FAILED(hr))
                    {
                        error = hresultError(
                            "Não foi possível criar o perfil AppContainer", hr);
                        return false;
                    }

                    created = true;
                    return true;
                }
                catch (const std::exception &exception)
                {
                    error = exception.what();
                    return false;
                }
            }

            bool cleanup(std::string &error)
            {
                bool ok = true;

                if (created && !name.empty())
                {
                    const HRESULT hr = DeleteAppContainerProfile(name.c_str());
                    if (FAILED(hr))
                    {
                        error = hresultError(
                            "Falha ao remover o perfil AppContainer", hr);
                        ok = false;
                    }
                    else
                    {
                        created = false;
                    }
                }

                if (sid)
                {
                    FreeSid(sid);
                    sid = nullptr;
                }

                return ok;
            }

        private:
            void cleanupNoThrow() noexcept
            {
                if (created && !name.empty())
                    DeleteAppContainerProfile(name.c_str());
                created = false;

                if (sid)
                {
                    FreeSid(sid);
                    sid = nullptr;
                }
            }
        };

        struct TempDirectory
        {
            fs::path path;

            TempDirectory() = default;
            TempDirectory(const TempDirectory &) = delete;
            TempDirectory &operator=(const TempDirectory &) = delete;

            ~TempDirectory()
            {
                removeNoThrow();
            }

            bool create(std::string &error)
            {
                try
                {
                    DWORD capacity = MAX_PATH;
                    std::vector<wchar_t> buffer;
                    DWORD length = 0;

                    for (int attempt = 0; attempt < 3; ++attempt)
                    {
                        buffer.assign(static_cast<std::size_t>(capacity) + 1, L'\0');
                        length = GetTempPathW(
                            static_cast<DWORD>(buffer.size()), buffer.data());

                        if (length == 0)
                        {
                            error = winError("Não foi possível obter o diretório temporário");
                            return false;
                        }

                        if (length < buffer.size())
                            break;

                        capacity = length + 1;
                    }

                    if (length == 0 || length >= buffer.size())
                    {
                        error = "O caminho do diretório temporário é excessivamente longo.";
                        return false;
                    }

                    const fs::path base(std::wstring(buffer.data(), length));

                    for (int attempt = 0; attempt < 8; ++attempt)
                    {
                        const fs::path candidate =
                            base / (L"IncubeAgent-" + makeGuidString());

                        if (CreateDirectoryW(candidate.c_str(), nullptr))
                        {
                            path = candidate;
                            return true;
                        }

                        const DWORD code = GetLastError();
                        if (code != ERROR_ALREADY_EXISTS)
                        {
                            error = winError(
                                "Não foi possível criar o diretório isolado", code);
                            return false;
                        }
                    }

                    error = "Não foi possível gerar um diretório temporário exclusivo.";
                    return false;
                }
                catch (const std::exception &exception)
                {
                    error = exception.what();
                    return false;
                }
            }

            bool remove(std::string &error)
            {
                if (path.empty())
                    return true;

                std::error_code ec;
                fs::remove_all(path, ec);
                if (ec)
                {
                    error = "Falha ao remover diretório temporário: " + ec.message();
                    return false;
                }

                path.clear();
                return true;
            }

        private:
            void removeNoThrow() noexcept
            {
                if (!path.empty())
                {
                    std::error_code ignored;
                    fs::remove_all(path, ignored);
                    path.clear();
                }
            }
        };

        bool setAclEntry(
            const fs::path &path,
            PSID sid,
            DWORD permissions,
            ACCESS_MODE mode,
            DWORD inheritance,
            std::string &error)
        {
            EXPLICIT_ACCESSW access{};
            access.grfAccessPermissions = permissions;
            access.grfAccessMode = mode;
            access.grfInheritance = inheritance;
            access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
            access.Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
            access.Trustee.ptstrName = reinterpret_cast<LPWSTR>(sid);

            PACL oldAcl = nullptr;
            PACL newAcl = nullptr;
            PSECURITY_DESCRIPTOR descriptor = nullptr;

            DWORD status = GetNamedSecurityInfoW(
                const_cast<LPWSTR>(path.c_str()),
                SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION,
                nullptr,
                nullptr,
                &oldAcl,
                nullptr,
                &descriptor);

            if (status != ERROR_SUCCESS)
            {
                error = "Falha ao consultar ACL: " + std::to_string(status);
                return false;
            }

            status = SetEntriesInAclW(1, &access, oldAcl, &newAcl);
            if (status != ERROR_SUCCESS)
            {
                if (descriptor)
                    LocalFree(descriptor);
                error = "Falha ao construir ACL: " + std::to_string(status);
                return false;
            }

            status = SetNamedSecurityInfoW(
                const_cast<LPWSTR>(path.c_str()),
                SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION,
                nullptr,
                nullptr,
                newAcl,
                nullptr);

            if (newAcl)
                LocalFree(newAcl);
            if (descriptor)
                LocalFree(descriptor);

            if (status != ERROR_SUCCESS)
            {
                error = "Falha ao aplicar ACL: " + std::to_string(status);
                return false;
            }

            return true;
        }

        class ScopedAclGrants
        {
        public:
            ScopedAclGrants() = default;
            ScopedAclGrants(const ScopedAclGrants &) = delete;
            ScopedAclGrants &operator=(const ScopedAclGrants &) = delete;

            ~ScopedAclGrants()
            {
                try
                {
                    std::string ignored;
                    revoke(ignored);
                }
                catch (...)
                {
                }
            }

            bool grantTemporaryDirectory(
                const fs::path &directory,
                PSID sid,
                std::string &error)
            {
                sid_ = sid;
                return grantOne(
                    directory,
                    FILE_ALL_ACCESS,
                    SUB_CONTAINERS_AND_OBJECTS_INHERIT,
                    error);
            }

            bool grantToolchainTree(
                const fs::path &root,
                PSID sid,
                std::string &error)
            {
                sid_ = sid;

                std::error_code ec;
                if (!fs::is_directory(root, ec) || ec)
                {
                    error = "Diretório do toolchain não encontrado: " +
                            root.string();
                    return false;
                }

                const DWORD rootAttributes = GetFileAttributesW(root.c_str());
                if (rootAttributes == INVALID_FILE_ATTRIBUTES ||
                    (rootAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                {
                    error = "Diretório do toolchain inválido ou reparse point.";
                    return false;
                }

                const DWORD readExecute = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;
                if (!grantOne(root, readExecute, NO_INHERITANCE, error))
                    return false;

                fs::recursive_directory_iterator iterator(root, ec);
                if (ec)
                {
                    error = "Falha ao percorrer toolchain: " + ec.message();
                    return false;
                }

                const fs::recursive_directory_iterator end;
                for (; iterator != end; iterator.increment(ec))
                {
                    if (ec)
                    {
                        error = "Falha ao percorrer toolchain: " + ec.message();
                        return false;
                    }

                    const fs::path current = iterator->path();
                    const DWORD attributes = GetFileAttributesW(current.c_str());

                    if (attributes == INVALID_FILE_ATTRIBUTES)
                    {
                        error = winError("Não foi possível consultar item do toolchain");
                        return false;
                    }

                    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
                    {
                        if (attributes & FILE_ATTRIBUTE_DIRECTORY)
                            iterator.disable_recursion_pending();

                        error = "O toolchain contém reparse point; acesso recusado: " +
                                current.string();
                        return false;
                    }

                    if (!grantOne(current, readExecute, NO_INHERITANCE, error))
                        return false;
                }

                return true;
            }

            bool revoke(std::string &error)
            {
                if (!sid_ || granted_.empty())
                {
                    sid_ = nullptr;
                    return true;
                }

                std::vector<fs::path> failed;
                for (auto iterator = granted_.rbegin();
                     iterator != granted_.rend(); ++iterator)
                {
                    std::string itemError;
                    if (!setAclEntry(
                            *iterator,
                            sid_,
                            FILE_ALL_ACCESS,
                            REVOKE_ACCESS,
                            NO_INHERITANCE,
                            itemError))
                    {
                        if (error.empty())
                            error = itemError + " em " + iterator->string();
                        failed.push_back(*iterator);
                    }
                }

                std::reverse(failed.begin(), failed.end());
                granted_ = std::move(failed);

                if (granted_.empty())
                {
                    sid_ = nullptr;
                    return true;
                }

                return false;
            }

        private:
            PSID sid_ = nullptr;
            std::vector<fs::path> granted_;

            bool grantOne(
                const fs::path &path,
                DWORD permissions,
                DWORD inheritance,
                std::string &error)
            {
                if (!setAclEntry(
                        path,
                        sid_,
                        permissions,
                        GRANT_ACCESS,
                        inheritance,
                        error))
                {
                    return false;
                }

                granted_.push_back(path);
                return true;
            }
        };

        enum class ToolchainKind
        {
            C,
            Cpp,
            Python,
            Rust
        };

        struct Toolchain
        {
            ToolchainKind kind{};
            fs::path compiler;
            fs::path binaryDirectory;
            fs::path rootDirectory;
            fs::path msysUsrBin;
        };

        bool findExecutableOnPath(
            const std::wstring &executableName,
            fs::path &result,
            std::string &error)
        {
            const std::wstring pathValue = readEnvironmentVariable(L"PATH");
            if (pathValue.empty())
            {
                error = "A variável PATH do processo está vazia.";
                return false;
            }

            std::size_t start = 0;
            while (start <= pathValue.size())
            {
                const std::size_t separator = pathValue.find(L';', start);
                std::wstring entry = pathValue.substr(
                    start,
                    separator == std::wstring::npos
                        ? std::wstring::npos
                        : separator - start);

                if (entry.size() >= 2 &&
                    entry.front() == L'"' && entry.back() == L'"')
                {
                    entry = entry.substr(1, entry.size() - 2);
                }

                if (!entry.empty())
                {
                    fs::path directory(entry);

                    // Entradas relativas poderiam resolver para o diretório atual.
                    if (directory.is_absolute())
                    {
                        const fs::path candidate = directory / executableName;
                        const DWORD attributes = GetFileAttributesW(candidate.c_str());

                        if (attributes != INVALID_FILE_ATTRIBUTES &&
                            !(attributes & FILE_ATTRIBUTE_DIRECTORY))
                        {
                            std::error_code ec;
                            fs::path canonical = fs::canonical(candidate, ec);
                            if (!ec)
                            {
                                result = std::move(canonical);
                                return true;
                            }
                        }
                    }
                }

                if (separator == std::wstring::npos)
                    break;
                start = separator + 1;
            }

            error = "Executável não encontrado em diretórios absolutos do PATH: " +
                    std::string(executableName.begin(), executableName.end());
            return false;
        }

        bool discoverToolchain(
            const fs::path &source,
            Toolchain &toolchain,
            std::string &error)
        {
            const std::wstring extension = lower(source.extension().wstring());
            std::wstring executable;

            if (extension == L".cpp")
            {
                toolchain.kind = ToolchainKind::Cpp;
                executable = L"g++.exe";
            }
            else if (extension == L".c")
            {
                toolchain.kind = ToolchainKind::C;
                executable = L"gcc.exe";
            }
            else if (extension == L".py")
            {
                toolchain.kind = ToolchainKind::Python;
                executable = L"python.exe";
            }
            else if (extension == L".rs")
            {
                toolchain.kind = ToolchainKind::Rust;
                executable = L"rustc.exe";
            }
            else
            {
                error = "Extensão não suportada pelo backend Windows.";
                return false;
            }

            if (!findExecutableOnPath(executable, toolchain.compiler, error))
                return false;

            const std::wstring compilerPath = lower(toolchain.compiler.wstring());
            if (compilerPath.find(L"\\windowsapps\\") != std::wstring::npos)
            {
                error = "O executável encontrado é um alias da Microsoft Store, não um runtime Python utilizável.";
                return false;
            }

            toolchain.binaryDirectory = toolchain.compiler.parent_path();

            switch (toolchain.kind)
            {
            case ToolchainKind::C:
            case ToolchainKind::Cpp:
            {
                toolchain.rootDirectory = toolchain.binaryDirectory.parent_path();
                const fs::path msysRoot = toolchain.rootDirectory.parent_path();
                toolchain.msysUsrBin = msysRoot / L"usr" / L"bin";

                std::error_code ec;
                if (!fs::is_directory(toolchain.rootDirectory / L"lib" / L"gcc", ec) || ec ||
                    !fs::is_directory(toolchain.msysUsrBin, ec) || ec)
                {
                    error = "O GCC encontrado não parece pertencer a uma instalação MSYS2/UCRT64 suportada.";
                    return false;
                }
                break;
            }
            case ToolchainKind::Python:
            {
                toolchain.rootDirectory = toolchain.binaryDirectory;
                std::error_code ec;
                if (!fs::is_directory(toolchain.rootDirectory / L"Lib", ec) || ec)
                {
                    error = "A instalação Python encontrada não contém a pasta Lib ao lado de python.exe.";
                    return false;
                }
                break;
            }
            case ToolchainKind::Rust:
            {
                if (compilerPath.find(L"\\.cargo\\bin\\") != std::wstring::npos)
                {
                    error = "O rustc encontrado é um proxy do rustup (.cargo\\bin). Este backend ainda exige o rustc direto do diretório toolchain.";
                    return false;
                }

                toolchain.rootDirectory = toolchain.binaryDirectory.parent_path();
                std::error_code ec;
                if (!fs::is_directory(
                        toolchain.rootDirectory / L"lib" / L"rustlib", ec) ||
                    ec)
                {
                    error = "Não foi possível identificar o sysroot do rustc encontrado.";
                    return false;
                }
                break;
            }
            }

            return true;
        }

        bool grantToolchainAccess(
            const Toolchain &toolchain,
            ScopedAclGrants &acl,
            PSID sid,
            std::string &error)
        {
            if (toolchain.kind == ToolchainKind::C ||
                toolchain.kind == ToolchainKind::Cpp)
            {
                if (!acl.grantToolchainTree(
                        toolchain.rootDirectory, sid, error))
                    return false;

                if (!acl.grantToolchainTree(
                        toolchain.msysUsrBin, sid, error))
                    return false;

                return true;
            }

            return acl.grantToolchainTree(
                toolchain.rootDirectory, sid, error);
        }

        bool validateSourcePath(
            const Config &config,
            fs::path &workspace,
            fs::path &candidate,
            std::string &error)
        {
            try
            {
                if (config.workspace.empty() || config.sourceFile.empty())
                {
                    error = "Workspace e arquivo-fonte são obrigatórios.";
                    return false;
                }

                if (config.sourceFile.is_absolute())
                {
                    error = "O caminho do fonte deve ser relativo à workspace.";
                    return false;
                }

                workspace = fs::canonical(config.workspace);
                if (!fs::is_directory(workspace))
                {
                    error = "A workspace não é um diretório.";
                    return false;
                }

                candidate = (workspace / config.sourceFile).lexically_normal();
                if (!isInside(workspace, candidate) || candidate == workspace)
                {
                    error = "O arquivo-fonte está fora da workspace.";
                    return false;
                }

                fs::path relative = candidate.lexically_relative(workspace);
                fs::path current = workspace;
                std::size_t componentIndex = 0;
                const std::size_t componentCount = static_cast<std::size_t>(
                    std::distance(relative.begin(), relative.end()));

                for (const auto &component : relative)
                {
                    ++componentIndex;
                    current /= component;

                    const DWORD attributes = GetFileAttributesW(current.c_str());
                    if (attributes == INVALID_FILE_ATTRIBUTES)
                    {
                        error = winError("Arquivo-fonte não encontrado");
                        return false;
                    }

                    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
                    {
                        error = "Reparse points e symlinks não são permitidos no caminho do fonte.";
                        return false;
                    }

                    if (componentIndex < componentCount &&
                        !(attributes & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        error = "Um componente intermediário do caminho não é diretório.";
                        return false;
                    }
                }

                const DWORD finalAttributes = GetFileAttributesW(candidate.c_str());
                if (finalAttributes == INVALID_FILE_ATTRIBUTES ||
                    (finalAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    error = "O caminho informado não é um arquivo regular.";
                    return false;
                }

                return true;
            }
            catch (const std::exception &exception)
            {
                error = exception.what();
                return false;
            }
        }

        bool finalPathFromHandle(HANDLE handle, fs::path &path, std::string &error)
        {
            const DWORD required = GetFinalPathNameByHandleW(
                handle,
                nullptr,
                0,
                FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);

            if (required == 0)
            {
                error = winError("Não foi possível resolver o caminho final do handle");
                return false;
            }

            std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 2);
            const DWORD length = GetFinalPathNameByHandleW(
                handle,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);

            if (length == 0 || length >= buffer.size())
            {
                error = winError("Não foi possível obter o caminho final do handle");
                return false;
            }

            path = fs::path(std::wstring(buffer.data(), length));
            return true;
        }

        bool copySourceIntoSandbox(
            const fs::path &workspace,
            const fs::path &source,
            const fs::path &destination,
            std::string &error)
        {
            Handle workspaceHandle(CreateFileW(
                workspace.c_str(),
                FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS,
                nullptr));

            if (!workspaceHandle)
            {
                error = winError("Não foi possível abrir a workspace");
                return false;
            }

            Handle sourceHandle(CreateFileW(
                source.c_str(),
                GENERIC_READ | FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN,
                nullptr));

            if (!sourceHandle)
            {
                error = winError("Não foi possível abrir o arquivo-fonte com segurança");
                return false;
            }

            BY_HANDLE_FILE_INFORMATION fileInformation{};
            if (!GetFileInformationByHandle(sourceHandle.get(), &fileInformation))
            {
                error = winError("Não foi possível consultar o arquivo-fonte aberto");
                return false;
            }

            if ((fileInformation.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
                (fileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                error = "O arquivo-fonte é um reparse point ou não é arquivo regular.";
                return false;
            }

            fs::path finalWorkspace;
            fs::path finalSource;
            if (!finalPathFromHandle(workspaceHandle.get(), finalWorkspace, error) ||
                !finalPathFromHandle(sourceHandle.get(), finalSource, error))
            {
                return false;
            }

            if (!isInside(finalWorkspace, finalSource) ||
                finalWorkspace == finalSource)
            {
                error = "O handle do arquivo-fonte resolve para fora da workspace.";
                return false;
            }

            LARGE_INTEGER fileSize{};
            if (!GetFileSizeEx(sourceHandle.get(), &fileSize))
            {
                error = winError("Não foi possível consultar o tamanho do fonte");
                return false;
            }

            if (fileSize.QuadPart < 0 ||
                static_cast<std::uint64_t>(fileSize.QuadPart) > MAX_SOURCE_SIZE)
            {
                error = "Arquivo-fonte excede o limite de 2 MB.";
                return false;
            }

            Handle destinationHandle(CreateFileW(
                destination.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL,
                nullptr));

            if (!destinationHandle)
            {
                error = winError("Não foi possível criar a cópia isolada do fonte");
                return false;
            }

            std::array<char, 64 * 1024> buffer{};
            for (;;)
            {
                DWORD bytesRead = 0;
                if (!ReadFile(
                        sourceHandle.get(),
                        buffer.data(),
                        static_cast<DWORD>(buffer.size()),
                        &bytesRead,
                        nullptr))
                {
                    error = winError("Falha ao ler o arquivo-fonte");
                    return false;
                }

                if (bytesRead == 0)
                    break;

                DWORD offset = 0;
                while (offset < bytesRead)
                {
                    DWORD bytesWritten = 0;
                    if (!WriteFile(
                            destinationHandle.get(),
                            buffer.data() + offset,
                            bytesRead - offset,
                            &bytesWritten,
                            nullptr) ||
                        bytesWritten == 0)
                    {
                        error = winError("Falha ao copiar o fonte para o sandbox");
                        return false;
                    }
                    offset += bytesWritten;
                }
            }

            return true;
        }

        std::wstring quoteArgument(const std::wstring &argument)
        {
            std::wstring result = L"\"";
            std::size_t backslashes = 0;

            for (wchar_t ch : argument)
            {
                if (ch == L'\\')
                {
                    ++backslashes;
                    continue;
                }

                if (ch == L'"')
                {
                    result.append(backslashes * 2 + 1, L'\\');
                    result += L'"';
                    backslashes = 0;
                    continue;
                }

                result.append(backslashes, L'\\');
                backslashes = 0;
                result += ch;
            }

            result.append(backslashes * 2, L'\\');
            result += L'"';
            return result;
        }

        std::wstring buildCommandLine(
            const std::vector<std::wstring> &arguments)
        {
            std::wstring commandLine;
            for (const auto &argument : arguments)
            {
                if (!commandLine.empty())
                    commandLine += L' ';
                commandLine += quoteArgument(argument);
            }
            return commandLine;
        }

        std::vector<wchar_t> buildEnvironmentBlock(
            const Toolchain &toolchain,
            const fs::path &temporaryDirectory)
        {
            std::map<std::wstring, std::wstring> variables;
            const std::wstring windowsDirectory = getWindowsDirectory();

            std::wstring path = toolchain.binaryDirectory.wstring();
            if (!toolchain.msysUsrBin.empty())
                path += L";" + toolchain.msysUsrBin.wstring();
            path += L";" + (fs::path(windowsDirectory) / L"System32").wstring();
            path += L";" + windowsDirectory;

            variables.emplace(L"SystemRoot", windowsDirectory);
            variables.emplace(L"WINDIR", windowsDirectory);
            variables.emplace(L"PATH", path);
            variables.emplace(L"TEMP", temporaryDirectory.wstring());
            variables.emplace(L"TMP", temporaryDirectory.wstring());

            std::vector<wchar_t> block;
            for (const auto &[key, value] : variables)
            {
                const std::wstring entry = key + L"=" + value;
                block.insert(block.end(), entry.begin(), entry.end());
                block.push_back(L'\0');
            }
            block.push_back(L'\0');
            return block;
        }

        bool createAttributeList(
            LPPROC_THREAD_ATTRIBUTE_LIST &attributes,
            std::string &error)
        {
            SIZE_T required = 0;
            SetLastError(ERROR_SUCCESS);
            const BOOL first = InitializeProcThreadAttributeList(
                nullptr, 2, 0, &required);

            if (first || GetLastError() != ERROR_INSUFFICIENT_BUFFER || required == 0)
            {
                error = winError("Falha ao calcular tamanho da lista de atributos");
                return false;
            }

            attributes = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
                HeapAlloc(GetProcessHeap(), 0, required));

            if (!attributes)
            {
                error = "Memória insuficiente para lista de atributos do processo.";
                return false;
            }

            if (!InitializeProcThreadAttributeList(
                    attributes, 2, 0, &required))
            {
                const DWORD code = GetLastError();
                HeapFree(GetProcessHeap(), 0, attributes);
                attributes = nullptr;
                error = winError("Falha ao inicializar lista de atributos", code);
                return false;
            }

            return true;
        }

        void destroyAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST attributes) noexcept
        {
            if (!attributes)
                return;
            DeleteProcThreadAttributeList(attributes);
            HeapFree(GetProcessHeap(), 0, attributes);
        }

        bool configureJob(HANDLE job, const Config &config, std::string &error)
        {
            if (config.maxProcesses == 0 || config.maxProcesses > 64)
            {
                error = "maxProcesses deve estar entre 1 e 64.";
                return false;
            }

            if (config.memoryLimitMB < 16 ||
                config.memoryLimitMB > MAX_MEMORY_LIMIT_MB)
            {
                error = "memoryLimitMB deve estar entre 16 e 4096 MB.";
                return false;
            }

            const std::uint64_t memoryBytes64 =
                config.memoryLimitMB * 1024ULL * 1024ULL;

            if (memoryBytes64 > std::numeric_limits<SIZE_T>::max())
            {
                error = "Limite de memória não cabe no tamanho de endereço deste processo.";
                return false;
            }

            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags =
                JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
                JOB_OBJECT_LIMIT_PROCESS_MEMORY |
                JOB_OBJECT_LIMIT_JOB_MEMORY;

            limits.BasicLimitInformation.ActiveProcessLimit =
                static_cast<DWORD>(config.maxProcesses);

            limits.ProcessMemoryLimit = static_cast<SIZE_T>(memoryBytes64);
            limits.JobMemoryLimit = static_cast<SIZE_T>(memoryBytes64);

            if (!SetInformationJobObject(
                    job,
                    JobObjectExtendedLimitInformation,
                    &limits,
                    sizeof(limits)))
            {
                error = winError("Falha ao configurar limites do Job Object");
                return false;
            }

            return true;
        }

        bool terminateResidualProcesses(HANDLE job, std::string &error)
        {
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
            if (!QueryInformationJobObject(
                    job,
                    JobObjectBasicAccountingInformation,
                    &accounting,
                    sizeof(accounting),
                    nullptr))
            {
                error = winError("Falha ao consultar processos restantes no Job Object");
                return false;
            }

            if (accounting.ActiveProcesses == 0)
                return true;

            if (!TerminateJobObject(job, 0))
            {
                error = winError("Falha ao encerrar processos filhos restantes");
                return false;
            }

            const auto deadline = Clock::now() + std::chrono::seconds(3);
            do
            {
                accounting = {};
                if (!QueryInformationJobObject(
                        job,
                        JobObjectBasicAccountingInformation,
                        &accounting,
                        sizeof(accounting),
                        nullptr))
                {
                    error = winError("Falha ao consultar encerramento dos processos filhos");
                    return false;
                }

                if (accounting.ActiveProcesses == 0)
                    return true;

                Sleep(10);
            } while (Clock::now() < deadline);

            error = "Os processos filhos não terminaram após TerminateJobObject.";
            return false;
        }

        struct ProcessResult
        {
            bool started = false;
            bool timedOut = false;
            bool outputTruncated = false;
            DWORD exitCode = static_cast<DWORD>(-1);
            std::string output;
            std::string error;
        };

        ProcessResult runIsolatedProcess(
            PSID appContainerSid,
            HANDLE job,
            const fs::path &executable,
            const std::vector<std::wstring> &arguments,
            const fs::path &workingDirectory,
            const std::vector<wchar_t> &environment,
            Clock::time_point deadline,
            std::size_t outputLimit)
        {
            ProcessResult result;

            Handle readPipe;
            Handle writePipe;
            Handle stdinHandle;

            SECURITY_ATTRIBUTES inheritable{};
            inheritable.nLength = sizeof(inheritable);
            inheritable.bInheritHandle = TRUE;

            HANDLE rawRead = nullptr;
            HANDLE rawWrite = nullptr;
            if (!CreatePipe(&rawRead, &rawWrite, &inheritable, 0))
            {
                result.error = winError("Falha ao criar pipe de saída");
                return result;
            }

            readPipe.reset(rawRead);
            writePipe.reset(rawWrite);

            if (!SetHandleInformation(
                    readPipe.get(), HANDLE_FLAG_INHERIT, 0))
            {
                result.error = winError("Falha ao tornar a leitura do pipe não herdável");
                return result;
            }

            stdinHandle.reset(CreateFileW(
                L"NUL",
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                &inheritable,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr));

            if (!stdinHandle)
            {
                result.error = winError("Falha ao abrir stdin nulo");
                return result;
            }

            SECURITY_CAPABILITIES securityCapabilities{};
            securityCapabilities.AppContainerSid = appContainerSid;
            securityCapabilities.Capabilities = nullptr;
            securityCapabilities.CapabilityCount = 0;
            securityCapabilities.Reserved = 0;

            LPPROC_THREAD_ATTRIBUTE_LIST attributes = nullptr;
            if (!createAttributeList(attributes, result.error))
                return result;

            struct AttributeGuard
            {
                LPPROC_THREAD_ATTRIBUTE_LIST value = nullptr;
                ~AttributeGuard() { destroyAttributeList(value); }
            } attributeGuard{attributes};

            HANDLE inheritedHandles[] = {
                writePipe.get(),
                stdinHandle.get()};

            if (!UpdateProcThreadAttribute(
                    attributes,
                    0,
                    PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
                    &securityCapabilities,
                    sizeof(securityCapabilities),
                    nullptr,
                    nullptr))
            {
                result.error = winError("Falha ao configurar token AppContainer");
                return result;
            }

            if (!UpdateProcThreadAttribute(
                    attributes,
                    0,
                    PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                    inheritedHandles,
                    sizeof(inheritedHandles),
                    nullptr,
                    nullptr))
            {
                result.error = winError("Falha ao restringir handles herdados");
                return result;
            }

            std::vector<std::wstring> commandArguments;
            commandArguments.reserve(arguments.size() + 1);
            commandArguments.push_back(executable.wstring());
            commandArguments.insert(
                commandArguments.end(), arguments.begin(), arguments.end());

            std::wstring commandLine = buildCommandLine(commandArguments);
            const std::wstring executablePath = executable.wstring();
            const std::wstring workingPath = workingDirectory.wstring();

            STARTUPINFOEXW startup{};
            startup.StartupInfo.cb = sizeof(startup);
            startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
            startup.StartupInfo.hStdInput = stdinHandle.get();
            startup.StartupInfo.hStdOutput = writePipe.get();
            startup.StartupInfo.hStdError = writePipe.get();
            startup.lpAttributeList = attributes;

            PROCESS_INFORMATION processInformation{};
            const DWORD creationFlags =
                EXTENDED_STARTUPINFO_PRESENT |
                CREATE_SUSPENDED |
                CREATE_NO_WINDOW |
                CREATE_UNICODE_ENVIRONMENT;

            if (!CreateProcessW(
                    executablePath.c_str(),
                    commandLine.data(),
                    nullptr,
                    nullptr,
                    TRUE,
                    creationFlags,
                    const_cast<wchar_t *>(environment.data()),
                    workingPath.c_str(),
                    &startup.StartupInfo,
                    &processInformation))
            {
                result.error = winError("Falha ao criar processo no AppContainer");
                return result;
            }

            Handle process(processInformation.hProcess);
            Handle primaryThread(processInformation.hThread);
            result.started = true;

            if (!AssignProcessToJobObject(job, process.get()))
            {
                const DWORD code = GetLastError();
                TerminateProcess(process.get(), 1);
                WaitForSingleObject(process.get(), INFINITE);
                result.error = winError("Falha ao associar processo ao Job Object", code);
                return result;
            }

            if (ResumeThread(primaryThread.get()) == static_cast<DWORD>(-1))
            {
                const DWORD code = GetLastError();
                TerminateJobObject(job, 1);
                WaitForSingleObject(process.get(), INFINITE);
                result.error = winError("Falha ao retomar processo suspenso", code);
                return result;
            }

            // O filho já recebeu cópias dos handles permitidos.
            writePipe.reset();
            stdinHandle.reset();

            bool processExited = false;
            bool pipeOpen = true;
            bool timeoutTriggered = false;
            std::array<char, 4096> buffer{};

            while (pipeOpen || !processExited)
            {
                if (!timeoutTriggered && Clock::now() >= deadline)
                {
                    timeoutTriggered = true;
                    result.timedOut = true;
                    TerminateJobObject(job, 124);
                }

                if (!processExited)
                {
                    const DWORD wait = WaitForSingleObject(process.get(), 10);
                    if (wait == WAIT_OBJECT_0)
                    {
                        processExited = true;
                        if (!GetExitCodeProcess(process.get(), &result.exitCode))
                            result.error = winError("Falha ao obter código de saída");
                    }
                    else if (wait == WAIT_FAILED)
                    {
                        const DWORD code = GetLastError();
                        if (result.error.empty())
                            result.error = winError("Falha ao aguardar processo", code);
                        TerminateJobObject(job, 1);
                        WaitForSingleObject(process.get(), INFINITE);
                        processExited = true;
                        GetExitCodeProcess(process.get(), &result.exitCode);
                    }
                }

                if (!pipeOpen)
                    continue;

                DWORD available = 0;
                if (!PeekNamedPipe(
                        readPipe.get(),
                        nullptr,
                        0,
                        nullptr,
                        &available,
                        nullptr))
                {
                    const DWORD code = GetLastError();
                    if (code == ERROR_BROKEN_PIPE)
                    {
                        pipeOpen = false;
                        continue;
                    }

                    if (result.error.empty())
                        result.error = winError("Falha ao consultar pipe", code);
                    TerminateJobObject(job, 1);
                    pipeOpen = false;
                    continue;
                }

                if (available == 0)
                {
                    // Pipe vazio não é EOF: um processo descendente pode manter
                    // o handle de escrita aberto mesmo depois do processo pai sair.
                    Sleep(10);
                    continue;
                }

                DWORD bytesRead = 0;
                const DWORD toRead = static_cast<DWORD>(
                    std::min<std::size_t>(available, buffer.size()));

                if (!ReadFile(
                        readPipe.get(),
                        buffer.data(),
                        toRead,
                        &bytesRead,
                        nullptr))
                {
                    const DWORD code = GetLastError();
                    if (code == ERROR_BROKEN_PIPE)
                    {
                        pipeOpen = false;
                        continue;
                    }

                    if (result.error.empty())
                        result.error = winError("Falha ao ler stdout/stderr", code);
                    TerminateJobObject(job, 1);
                    pipeOpen = false;
                    continue;
                }

                if (bytesRead == 0)
                    continue;

                const std::size_t remaining =
                    outputLimit > result.output.size()
                        ? outputLimit - result.output.size()
                        : 0;

                const std::size_t stored = std::min(
                    remaining,
                    static_cast<std::size_t>(bytesRead));

                if (stored > 0)
                    result.output.append(buffer.data(), stored);

                if (stored < bytesRead)
                    result.outputTruncated = true;
            }

            std::string residualError;
            if (!terminateResidualProcesses(job, residualError) && result.error.empty())
                result.error = residualError;

            if (result.timedOut)
                result.exitCode = 124;

            return result;
        }

        void appendProcessResult(
            Result &destination,
            const ProcessResult &source)
        {
            destination.output += source.output;
            destination.outputTruncated =
                destination.outputTruncated || source.outputTruncated;
        }

        std::vector<std::wstring> makeCompileArguments(
            const Toolchain &toolchain,
            const fs::path &source,
            const fs::path &program)
        {
            switch (toolchain.kind)
            {
            case ToolchainKind::C:
                return {
                    L"-std=c17",
                    L"-O2",
                    source.wstring(),
                    L"-o",
                    program.wstring()};

            case ToolchainKind::Cpp:
                return {
                    L"-std=c++20",
                    L"-O2",
                    source.wstring(),
                    L"-o",
                    program.wstring()};

            case ToolchainKind::Rust:
                return {
                    L"--edition=2021",
                    source.wstring(),
                    L"-o",
                    program.wstring()};

            case ToolchainKind::Python:
                break;
            }

            return {};
        }

        bool validateConfig(const Config &config, std::string &error)
        {
            if (config.timeout < std::chrono::seconds(1) ||
                config.timeout > std::chrono::seconds(30))
            {
                error = "O timeout deve estar entre 1 e 30 segundos.";
                return false;
            }

            if (config.maxOutputBytes > MAX_OUTPUT_LIMIT)
            {
                error = "maxOutputBytes não pode exceder 1 MB.";
                return false;
            }

            return true;
        }
    }

    Result run(const Config &config)
    {
        Result result{};

        try
        {
            if (!validateConfig(config, result.error))
                return result;

            fs::path workspace;
            fs::path source;
            if (!validateSourcePath(config, workspace, source, result.error))
                return result;

            Toolchain toolchain;
            if (!discoverToolchain(source, toolchain, result.error))
                return result;

            AppContainer appContainer;
            if (!appContainer.create(result.error))
                return result;

            TempDirectory temporaryDirectory;
            if (!temporaryDirectory.create(result.error))
                return result;

            ScopedAclGrants aclGrants;
            if (!aclGrants.grantTemporaryDirectory(
                    temporaryDirectory.path,
                    appContainer.sid,
                    result.error))
            {
                return result;
            }

            if (!grantToolchainAccess(
                    toolchain,
                    aclGrants,
                    appContainer.sid,
                    result.error))
            {
                return result;
            }

            const std::wstring extension = lower(source.extension().wstring());
            const fs::path stagedSource = temporaryDirectory.path / (L"source" + extension);
            if (!copySourceIntoSandbox(
                    workspace,
                    source,
                    stagedSource,
                    result.error))
            {
                return result;
            }

            const std::vector<wchar_t> environment =
                buildEnvironmentBlock(toolchain, temporaryDirectory.path);

            Handle job(CreateJobObjectW(nullptr, nullptr));
            if (!job)
            {
                result.error = winError("Falha ao criar Job Object");
                return result;
            }

            if (!configureJob(job.get(), config, result.error))
                return result;

            const Clock::time_point deadline = Clock::now() + config.timeout;
            const fs::path program = temporaryDirectory.path / L"program.exe";

            if (toolchain.kind == ToolchainKind::Python)
            {
                const std::vector<std::wstring> arguments = {
                    L"-I",
                    L"-B",
                    stagedSource.wstring()};

                ProcessResult execution = runIsolatedProcess(
                    appContainer.sid,
                    job.get(),
                    toolchain.compiler,
                    arguments,
                    temporaryDirectory.path,
                    environment,
                    deadline,
                    config.maxOutputBytes);

                appendProcessResult(result, execution);
                result.timedOut = execution.timedOut;
                result.exitCode = execution.timedOut
                                      ? 124
                                      : static_cast<int>(execution.exitCode);

                if (!execution.error.empty())
                    result.error = execution.error;
            }
            else
            {
                const std::vector<std::wstring> compileArguments =
                    makeCompileArguments(toolchain, stagedSource, program);

                ProcessResult compilation = runIsolatedProcess(
                    appContainer.sid,
                    job.get(),
                    toolchain.compiler,
                    compileArguments,
                    temporaryDirectory.path,
                    environment,
                    deadline,
                    config.maxOutputBytes);

                appendProcessResult(result, compilation);
                result.timedOut = compilation.timedOut;
                result.exitCode = compilation.timedOut
                                      ? 124
                                      : static_cast<int>(compilation.exitCode);

                if (!compilation.error.empty())
                    result.error = compilation.error;

                if (result.error.empty() && !result.timedOut &&
                    compilation.started && compilation.exitCode == 0)
                {
                    const std::size_t remaining =
                        config.maxOutputBytes > result.output.size()
                            ? config.maxOutputBytes - result.output.size()
                            : 0;

                    const std::vector<std::wstring> runArguments{};
                    ProcessResult execution = runIsolatedProcess(
                        appContainer.sid,
                        job.get(),
                        program,
                        runArguments,
                        temporaryDirectory.path,
                        environment,
                        deadline,
                        remaining);

                    appendProcessResult(result, execution);
                    result.timedOut = execution.timedOut;
                    result.exitCode = execution.timedOut
                                          ? 124
                                          : static_cast<int>(execution.exitCode);

                    if (!execution.error.empty())
                        result.error = execution.error;
                }
            }

            // Primeiro fecha o Job (matando qualquer descendente restante),
            // depois revoga as ACLs, remove os arquivos e exclui o perfil.
            job.reset();

            std::string cleanupError;
            const bool aclClean = aclGrants.revoke(cleanupError);
            if (!aclClean)
            {
                if (result.error.empty())
                    result.error = "Falha na limpeza das permissões temporárias: " + cleanupError;
                // Não liberar o SID enquanto a classe de ACL ainda pode precisar
                // dele para uma segunda tentativa de revogação no destrutor.
            }
            else
            {
                cleanupError.clear();
                if (!temporaryDirectory.remove(cleanupError) && result.error.empty())
                    result.error = cleanupError;

                cleanupError.clear();
                if (!appContainer.cleanup(cleanupError) && result.error.empty())
                    result.error = cleanupError;
            }

            result.success =
                result.error.empty() &&
                !result.timedOut &&
                result.exitCode == 0;

            return result;
        }
        catch (const fs::filesystem_error &exception)
        {
            result.success = false;
            result.error = std::string("Erro de filesystem: ") + exception.what();
            return result;
        }
        catch (const std::exception &exception)
        {
            result.success = false;
            result.error = exception.what();
            return result;
        }
        catch (...)
        {
            result.success = false;
            result.error = "Erro desconhecido no backend Windows do sandbox.";
            return result;
        }
    }
}

#endif // _WIN32
