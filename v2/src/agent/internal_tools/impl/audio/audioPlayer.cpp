#include "internalTools.h"

ToolResult InternalTools::audioPlayer(const std::string &arguments)
{
    try
    {
        if (!tts.speak(arguments))
        {
            return {
                false,
                "",
                "Não foi possível processar o TTS"};
        }

        return {
            true,
            "TTS concluído",
            ""};
    }
    catch (const std::exception &e)
    {
        return {
            false,
            "",
            e.what()};
    }
}
