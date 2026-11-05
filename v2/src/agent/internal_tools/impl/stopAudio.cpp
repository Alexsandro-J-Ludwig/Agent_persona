#include "internalTools.h"

ToolResult InternalTools::stopAudio(const std::string &arguments)
{
    try
    {
        tts.stop();
        return {true, "Reprodução de áudio interrompida.", ""};
    }
    catch (const std::exception &e)
    {
        return {false, "", e.what()};
    }
}