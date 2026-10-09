
cmake_minimum_required(VERSION 3.25)

project(
    IncubeAgent
    VERSION 0.1.0
    LANGUAGES C CXX
)

# ============================================================
# C++
# ============================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Gera compile_commands.json para VS Code / clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# DEPENDÊNCIAS
# ============================================================

include(FetchContent)

# -------------------------
# FTXUI
# -------------------------

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v7.0.3
)

set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)


FetchContent_MakeAvailable(ftxui)

# -------------------------
# fmt
# cpp-rich depende dele
# -------------------------

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 12.2.0
)

set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_DOC OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(fmt)

# -------------------------
# MD4C
# Parser Markdown / CommonMark
# -------------------------

FetchContent_Declare(
    md4c
    GIT_REPOSITORY https://github.com/mity/md4c.git
    GIT_TAG release-0.4.8
    GIT_SHALLOW TRUE
)

# Desabilita componentes extras do MD4C
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(MD4C_BUILD_TESTS OFF CACHE BOOL "" FORCE)

set(CMAKE_POLICY_VERSION_MINIMUM "3.5")

FetchContent_MakeAvailable(md4c)

# -------------------------
# OpenSSL
# Disponível no MSYS2/UCRT64
# -------------------------

find_package(OpenSSL REQUIRED)

# ============================================================
# EXECUTÁVEL
# ============================================================

add_executable(agent

    # Entrada
    src/main.cpp

    # Configurações
    src/configs/models.cpp
    src/configs/dotenv.cpp
    src/configs/httplibconfig.cpp

    # Ollama
    src/ollama/ollama.cpp

    # Agente
    src/agent/orchestrator/agentOrchestrator.cpp
    src/agent/routing/router.cpp
    src/agent/history/historyManager.cpp

    # Chat
    src/chat/chat.cpp

    # Terminal
    src/terminal/terminal.cpp

    # Markdown
    src/markdown/markdownRenderer.cpp
    src/markdown/markdownFtxui.cpp
)

# ============================================================
# HEADERS LOCAIS
# ============================================================

target_include_directories(
    agent
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/libs

        # Headers do MD4C
        ${md4c_SOURCE_DIR}/src
)

# ============================================================
# CPP-HTTPLIB
# ============================================================

target_compile_definitions(
    agent
    PRIVATE
        CPPHTTPLIB_OPENSSL_SUPPORT
)

# ============================================================
# BIBLIOTECAS
# ============================================================

target_link_libraries(
    agent
    PRIVATE

        # Interface do terminal
        ftxui::ftxui

        # Formatação
        fmt::fmt

        # Parser Markdown
        md4c

        # HTTPS
        OpenSSL::SSL
        OpenSSL::Crypto

        # Bibliotecas Windows
        ws2_32
        crypt32
)

# ============================================================
# WINDOWS
# ============================================================

if(WIN32)
    target_compile_definitions(
        agent
        PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
    )
endif()

# ============================================================
# TESTES OPCIONAIS DO MARKDOWN
# ============================================================

option(BUILD_MARKDOWN_TESTS "Compilar testes do parser Markdown" OFF)

if(BUILD_MARKDOWN_TESTS)
    add_executable(markdown_parser_test
        src/markdown/markdownRenderer.cpp
        tests/testMarkdown.cpp
    )

    target_include_directories(
        markdown_parser_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${md4c_SOURCE_DIR}/src
    )

    target_link_libraries(
        markdown_parser_test
        PRIVATE
            md4c
    )
endif()
