cmake_minimum_required(VERSION 3.25)

project(
    IncubeAgent
    VERSION 0.1.0
    LANGUAGES CXX
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

# Não precisamos dos exemplos/testes do FTXUI
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
# OpenSSL
# Já existe no MSYS2/UCRT64
# -------------------------

find_package(OpenSSL REQUIRED)

# ============================================================
# EXECUTÁVEL
# ============================================================

add_executable(agent

    src/main.cpp

    src/configs/models.cpp
    src/configs/dotenv.cpp
    src/configs/httplibconfig.cpp

    src/ollama/ollama.cpp

    src/agent/orchestrator/agentOrchestrator.cpp
    src/agent/routing/router.cpp
    src/agent/history/historyManager.cpp

    src/chat/chat.cpp

    src/terminal/terminal.cpp

    # Futuramente:
    # terminal/terminal.cpp
)

# ============================================================
# HEADERS LOCAIS
# ============================================================

target_include_directories(
    agent
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/libs
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

        # Terminal UI
        ftxui::ftxui

        # cpp-rich
        fmt::fmt

        # HTTPS
        OpenSSL::SSL
        OpenSSL::Crypto

        # Windows
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