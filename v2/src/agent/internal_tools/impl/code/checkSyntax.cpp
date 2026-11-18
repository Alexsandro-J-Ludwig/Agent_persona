
#include "internalTools.h"
#include "definitions.h"

#include <boost/process/v1.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <vector>
#include <string>

namespace fs = std::filesystem;
namespace bp = boost::process::v1;

namespace
{
    struct ProcessResult
    {
        int exitCode = -1;
        std::string output;
        std::string error;
        bool executed = false;
    };

    // Executa um programa sem depender do shell.
    static ProcessResult runProcess(
        const std::string &executable,
        const std::vector<std::string> &arguments)
    {
        ProcessResult result;

        try
        {
            bp::ipstream stdoutStream;
            bp::ipstream stderrStream;

            bp::child process(
                executable,
                bp::args(arguments),
                bp::std_out > stdoutStream,
                bp::std_err > stderrStream);

            std::thread stdoutReader([&]()
                                     {
                std::ostringstream buffer;
                buffer << stdoutStream.rdbuf();
                result.output = buffer.str(); });

            std::thread stderrReader([&]()
                                     {
                std::ostringstream buffer;
                buffer << stderrStream.rdbuf();
                result.error = buffer.str(); });

            process.wait();

            result.exitCode = process.exit_code();
            result.executed = true;

            stdoutReader.join();
            stderrReader.join();
        }
        catch (const std::exception &e)
        {
            result.error = e.what();
        }

        return result;
    }

    // Localiza executáveis no PATH.
    static std::string findExecutable(
        const std::vector<std::string> &candidates)
    {
        for (const auto &candidate : candidates)
        {
            const auto path = bp::search_path(candidate);

            if (!path.empty())
                return path.string();
        }

        return {};
    }

    // Cria diretório temporário para arquivos intermediários.
    static fs::path createTempDirectory()
    {
        static std::atomic_uint64_t counter{0};

        const auto id = counter.fetch_add(1);

        const fs::path directory =
            fs::temp_directory_path() /
            ("IncubeAgent_Syntax_" + std::to_string(id));

        fs::create_directories(directory);

        return directory;
    }

    static std::string normalizeExtension(const fs::path &path)
    {
        std::string extension = path.extension().string();

        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

        return extension;
    }

    // Executa validadores que utilizam Python.
    static ProcessResult runPythonValidator(
        const fs::path &file,
        const std::string &script)
    {
        const std::string python = findExecutable(
            {"python3", "python"});

        if (python.empty())
        {
            return {
                -1,
                "",
                "Interpretador Python 3 não encontrado.",
                false};
        }

        return runProcess(
            python,
            {"-c",
             script,
             fs::absolute(file).string()});
    }

    // C e C++
    static ProcessResult analyzeCpp(const fs::path &file)
    {
        const std::string extension = normalizeExtension(file);

        const bool isC = extension == ".c";

        const std::string compiler = findExecutable(
            isC
                ? std::vector<std::string>{"clang"}
                : std::vector<std::string>{"clang++"});

        if (compiler.empty())
        {
            return {
                -1, "",
                isC
                    ? "Compilador Clang para C não encontrado."
                    : "Compilador Clang++ não encontrado.",
                false};
        }

        std::vector<std::string> args = {
            "-fsyntax-only",
            "-Wall",
            "-Wextra",
            isC ? "-std=c17" : "-std=c++20"};

        if (extension == ".h" || extension == ".hpp")
        {
            args.push_back("-x");
            args.push_back("c++-header");
        }

        args.push_back(fs::absolute(file).string());

        return runProcess(compiler, args);
    }

    // Python: análise sintática e semântica de tipos.
    static ProcessResult analyzePython(const fs::path &file)
    {
        const std::string pyright = findExecutable({"pyright"});

        if (pyright.empty())
        {
            return {
                -1, "",
                "Analisador Pyright não encontrado. "
                "Instale o Pyright para analisar Python.",
                false};
        }

        return runProcess(
            pyright,
            {"--outputjson",
             fs::absolute(file).string()});
    }

    // Java
    static ProcessResult analyzeJava(const fs::path &file)
    {
        const std::string compiler = findExecutable({"javac"});

        if (compiler.empty())
        {
            return {
                -1, "",
                "Compilador javac não encontrado. Instale um JDK.",
                false};
        }

        try
        {
            const fs::path temp = createTempDirectory();

            const auto result = runProcess(
                compiler,
                {"-proc:none",
                 "-d",
                 temp.string(),
                 fs::absolute(file).string()});

            fs::remove_all(temp);

            return result;
        }
        catch (const std::exception &e)
        {
            return {-1, "", e.what(), false};
        }
    }

    // Rust
    static ProcessResult analyzeRust(const fs::path &file)
    {
        const std::string compiler = findExecutable({"rustc"});

        if (compiler.empty())
        {
            return {
                -1, "",
                "Compilador Rust (rustc) não encontrado.",
                false};
        }

        try
        {
            const fs::path temp = createTempDirectory();
            const fs::path output = temp / "analysis.rmeta";

            const auto result = runProcess(
                compiler,
                {"--emit=metadata",
                 "-o",
                 output.string(),
                 fs::absolute(file).string()});

            fs::remove_all(temp);

            return result;
        }
        catch (const std::exception &e)
        {
            return {-1, "", e.what(), false};
        }
    }

    // C#
    static ProcessResult analyzeCSharp(const fs::path &file)
    {
        const std::string compiler = findExecutable(
            {"csc", "mcs"});

        if (compiler.empty())
        {
            return {
                -1, "",
                "Compilador C# não encontrado. "
                "Instale Roslyn (csc) ou Mono (mcs).",
                false};
        }

        try
        {
            const fs::path temp = createTempDirectory();
            const fs::path output = temp / "analysis.dll";

            const bool isMcs =
                fs::path(compiler).filename().string().find("mcs") != std::string::npos;

            const auto result = runProcess(
                compiler,
                isMcs
                    ? std::vector<std::string>{
                          "-target:library",
                          "-out:" + output.string(),
                          fs::absolute(file).string()}
                    : std::vector<std::string>{"/nologo", "/target:library", "/out:" + output.string(), fs::absolute(file).string()});

            fs::remove_all(temp);

            return result;
        }
        catch (const std::exception &e)
        {
            return {-1, "", e.what(), false};
        }
    }

    // XML
    static ProcessResult analyzeXml(const fs::path &file)
    {
        return runPythonValidator(
            file,
            R"(
import sys
import xml.etree.ElementTree as ET

ET.parse(sys.argv[1])
)");
    }

    // YAML
    static ProcessResult analyzeYaml(const fs::path &file)
    {
        return runPythonValidator(
            file,
            R"(
import sys
import yaml

with open(sys.argv[1], encoding="utf-8") as f:
    list(yaml.safe_load_all(f))
)");
    }

    // TOML
    static ProcessResult analyzeToml(const fs::path &file)
    {
        return runPythonValidator(
            file,
            R"(
import sys

try:
    import tomllib
except ImportError:
    import tomli as tomllib

with open(sys.argv[1], "rb") as f:
    tomllib.load(f)
)");
    }

    // INI
    static ProcessResult analyzeIni(const fs::path &file)
    {
        return runPythonValidator(
            file,
            R"(
import sys
import configparser

parser = configparser.ConfigParser(strict=True)
parser.read(sys.argv[1], encoding="utf-8")
)");
    }

    // CSV
    static ProcessResult analyzeCsv(const fs::path &file)
    {
        return runPythonValidator(
            file,
            R"(
import sys
import csv

with open(sys.argv[1], encoding="utf-8-sig", newline="") as f:
    reader = csv.reader(f, strict=True)
    rows = list(reader)

if rows:
    width = len(rows[0])
    if any(len(row) != width for row in rows):
        raise ValueError("Quantidade de colunas inconsistente")
)");
    }

    // Markdown
    static ProcessResult analyzeMarkdown(const fs::path &file)
    {
        const std::string analyzer =
            findExecutable({"markdownlint-cli2"});

        if (analyzer.empty())
        {
            return {
                -1, "",
                "Analisador markdownlint-cli2 não encontrado.",
                false};
        }

        return runProcess(
            analyzer,
            {fs::absolute(file).string()});
    }

    // JSON
    static ProcessResult analyzeJson(const fs::path &file)
    {
        try
        {
            std::ifstream input(file);

            if (!input.is_open())
                throw std::runtime_error("Não foi possível abrir o JSON.");

            json::parse(input);

            return {
                0,
                "JSON válido.",
                "",
                true};
        }
        catch (const std::exception &e)
        {
            return {
                1,
                "",
                e.what(),
                true};
        }
    }

} // namespace

ToolResult InternalTools::checkSyntax(
    const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);

        const fs::path file =
            args.at("path_code").get<std::string>();

        if (!fs::exists(file) || !fs::is_regular_file(file))
        {
            return {
                false,
                "",
                "Arquivo inexistente ou inválido."};
        }

        const std::string extension = normalizeExtension(file);

        if (!Definitions::allowedExt.contains(extension))
        {
            return {
                false,
                "",
                "Extensão não permitida: " + extension};
        }

        ProcessResult result;

        if (extension == ".c" ||
            extension == ".cpp" ||
            extension == ".h" ||
            extension == ".hpp")
        {
            result = analyzeCpp(file);
        }
        else if (extension == ".py")
        {
            result = analyzePython(file);
        }
        else if (extension == ".java")
        {
            result = analyzeJava(file);
        }
        else if (extension == ".rs")
        {
            result = analyzeRust(file);
        }
        else if (extension == ".cs")
        {
            result = analyzeCSharp(file);
        }
        else if (extension == ".json")
        {
            result = analyzeJson(file);
        }
        else if (extension == ".xml")
        {
            result = analyzeXml(file);
        }
        else if (extension == ".yaml" || extension == ".yml")
        {
            result = analyzeYaml(file);
        }
        else if (extension == ".toml")
        {
            result = analyzeToml(file);
        }
        else if (extension == ".ini")
        {
            result = analyzeIni(file);
        }
        else if (extension == ".csv")
        {
            result = analyzeCsv(file);
        }
        else if (extension == ".md")
        {
            result = analyzeMarkdown(file);
        }
        else
        {
            return {
                false,
                "",
                "Não existe analisador semântico implementado "
                "para a extensão: " +
                    extension};
        }

        if (!result.executed)
        {
            return {
                false,
                result.output,
                result.error};
        }

        if (result.exitCode != 0)
        {
            return {
                false,
                result.output,
                result.error.empty()
                    ? "O analisador encontrou erros."
                    : result.error};
        }

        return {
            true,
            result.output.empty()
                ? "Análise concluída com sucesso. "
                  "Código validado pelo analisador."
                : result.output,
            ""};
    }
    catch (const std::exception &e)
    {
        return {
            false,
            "",
            std::string("Erro ao verificar sintaxe: ") + e.what()};
    }
}
