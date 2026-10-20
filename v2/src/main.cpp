#include "./src/chat/chat.h"
#include "src/services/ttsService/ttsService.h"
#include <filesystem>
#include <iostream>

int main()
{
    TTSService ttsService;
    wchar_t executable[32768];

    const auto length = GetModuleFileNameW(nullptr, executable, 32768);

    if (length == 0 || length == 32768)
    {
        std::cerr << "Não foi possível localizar o servidor TTS. Continuando sem voz.\n";
    }
    else
    {
        auto root = std::filesystem::path(std::wstring(executable, length)).parent_path();
        const auto relative = std::filesystem::path("src/services/tts/dist/tts_server/tts_server.exe");

        std::error_code error;

        if (!std::filesystem::is_regular_file(root / relative, error))
            root = root.parent_path();

        if (!ttsService.start((root / relative).wstring()))
            std::cerr << "Falha ao iniciar o servidor TTS: " << GetLastError()
                      << ". Continuando sem voz.\n";
    }

    Chat chat;
    chat.run();

    return 0;
}
