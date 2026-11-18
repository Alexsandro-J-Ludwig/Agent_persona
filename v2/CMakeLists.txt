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

find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED IMPORTED_TARGET portaudio-2.0)
find_package(SndFile CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem system)


# Normaliza o prefixo MSYS2 (bin/../include) para não duplicar
# o diretório implícito do compilador e quebrar include_next.
foreach(AUDIO_DEPENDENCY IN ITEMS SndFile::sndfile PkgConfig::PORTAUDIO)
    get_target_property(AUDIO_INCLUDE_DIRS ${AUDIO_DEPENDENCY} INTERFACE_INCLUDE_DIRECTORIES)
    set(AUDIO_NORMALIZED_INCLUDE_DIRS "")
    foreach(AUDIO_INCLUDE_DIR IN LISTS AUDIO_INCLUDE_DIRS)
        cmake_path(NORMAL_PATH AUDIO_INCLUDE_DIR OUTPUT_VARIABLE AUDIO_NORMALIZED_INCLUDE_DIR)
        list(APPEND AUDIO_NORMALIZED_INCLUDE_DIRS "${AUDIO_NORMALIZED_INCLUDE_DIR}")
    endforeach()
    set_property(TARGET ${AUDIO_DEPENDENCY} PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES "${AUDIO_NORMALIZED_INCLUDE_DIRS}")
endforeach()

# ============================================================
# EXECUTÁVEL
# ============================================================

file(GLOB_RECURSE TOOL_SOURCES CONFIGURE_DEPENDS
    src/agent/internal_tools/impl/*.cpp)
list(FILTER TOOL_SOURCES EXCLUDE REGEX "sandbox_(linux|windows)\\.cpp$")

add_executable(agent
    src/main.cpp

    # Configurações
    src/configs/models.cpp
    src/configs/dotenv.cpp
    src/configs/httplibconfig.cpp
    src/configs/searxng.cpp

    # Ollama
    src/ollama/ollama.cpp

    # Agente
    src/agent/orchestrator/agentOrchestrator.cpp
    src/agent/orchestrator/speach/speach.cpp
    src/agent/routing/router.cpp
    src/agent/history/historyManager.cpp

    # Chat
    src/chat/chat.cpp

    # Terminal
    src/terminal/terminal.cpp

    # Markdown
    src/markdown/markdownRenderer.cpp
    src/markdown/markdownFtxui.cpp

    src/agent/internal_tools/internalTools.cpp
    ${TOOL_SOURCES}
    src/agent/internal_tools/audio/ttsService.cpp

    src/agent/memory/memory.cpp

    src/agent/tasks/taskManager.cpp
)

if(WIN32)
    target_sources(agent PRIVATE
        src/agent/internal_tools/impl/sandbox/sandbox_windows.cpp
    )
else()
    target_sources(agent PRIVATE
        src/agent/internal_tools/impl/sandbox/sandbox_linux.cpp
    )
endif()

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

        ${CMAKE_SOURCE_DIR}/src/agent/internal_tools
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
# BIBLIOTECAS (Geral)
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

        PkgConfig::PORTAUDIO
        SndFile::sndfile
        SQLite::SQLite3
        Boost::boost
        Boost::filesystem
        Boost::system
)

# ============================================================
# WINDOWS (Configurações e Bibliotecas exclusivas)
# ============================================================


if(WIN32)
    target_compile_definitions(
        agent
        PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _WIN32_WINNT=0x0A00
    )

    target_link_libraries(
        agent
        PRIVATE
            ws2_32
            crypt32
            advapi32
            userenv
            ole32
    )
endif()

if(UNIX AND NOT APPLE)
    find_program(BWRAP_EXECUTABLE bwrap)

    if(NOT BWRAP_EXECUTABLE)
        message(WARNING
            "Bubblewrap (bwrap) não encontrado. "
            "O sandbox Linux não poderá executar código."
        )
    else()
        message(STATUS "Bubblewrap encontrado: ${BWRAP_EXECUTABLE}")

        target_compile_definitions(
            agent
            PRIVATE
                SANDBOX_BWRAP_PATH="${BWRAP_EXECUTABLE}"
        )
    endif()
endif()