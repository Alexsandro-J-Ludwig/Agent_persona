
#include "audio.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
    const std::string audio_dir =
        std::string(std::getenv("HOME")) +
        "/documentos/Agent_persona/v2/src/agent/internal_tools/audio";

    const std::string cli =
        audio_dir + "/bin/qwen3-tts-cli";

    const std::string model =
        audio_dir + "/models/qwen3-tts-0.6b-f16.gguf";

    const std::string output =
        audio_dir + "/output.wav";
}

Audio::Audio() = default;

Audio::~Audio() {
    shutdown();
}

bool Audio::initialize() {
    if (!std::filesystem::exists(cli) ||
        !std::filesystem::exists(model)) {
        std::cerr << "[Audio] CLI ou modelo não encontrado.\n";
        return false;
    }

    initialized = true;
    std::cout << "[Audio] Módulo inicializado.\n";
    return true;
}

bool Audio::speak(const std::string& text) {
    if (!initialized || text.empty()) {
        return false;
    }

    // Versão inicial: chamar o CLI para gerar um WAV.
    // A passagem do texto será protegida contra argumentos
    // de shell na próxima etapa de integração.
    std::cerr
        << "[Audio] Precisamos conectar a geração do CLI: "
        << text << '\n';

    return false;
}

void Audio::shutdown() {
    initialized = false;
}
