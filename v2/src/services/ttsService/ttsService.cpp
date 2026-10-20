#include "ttsService.h"

bool TTSService::start(const std::wstring &exePath)
{
    if (process.hProcess)
        return true;

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);

    if (!CreateProcessW(
            exePath.c_str(),
            nullptr,
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process))
    {
        return false;
    }

    CloseHandle(process.hThread);
    process.hThread = nullptr;

    return true;
}

void TTSService::stop()
{
    if (!process.hProcess)
        return;

    if (WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT)
        TerminateProcess(process.hProcess, 0);

    CloseHandle(process.hProcess);
    process = {};
}

TTSService::~TTSService()
{
    stop();
}
