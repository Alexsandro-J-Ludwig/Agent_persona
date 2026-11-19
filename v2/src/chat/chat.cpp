#include "chat.h"

#include <exception>
#include <optional>
#include <string>
#include <thread>
#include <algorithm>
#include <string_view>

void Chat::sendMessage(const std::string &prompt)
{
    orquestrador.interruptSpeech();

    if (!orquestrador.submit(prompt))
    {
        terminal.addMessage("User", prompt);
        return;
    }

    if (ai_thread.joinable())
        ai_thread.join();

    terminal.addMessage("User", prompt);

    ai_thread = std::thread([this, prompt]()
                            {
       bool timerStarted = false;

std::optional<MessageId> current;
std::optional<MessageId> thinkCurrent;
std::optional<MessageId> researchCurrent;

// Estado do parser de pesquisa.
std::string streamBuffer;
bool insideResearch = false;

constexpr std::string_view openTag = "<RESEARCH>";
constexpr std::string_view closeTag = "</RESEARCH>";

// ============================================================
// SAÍDA NORMAL DA IA
// ============================================================

auto appendNormalText = [&](const std::string &content)
{
    if (content.empty())
        return;

    if (!current)
    {
        if (content.find_first_not_of(" \t\r\n") == std::string::npos)
            return;

        if (thinkCurrent)
        {
            terminal.finishMessage(*thinkCurrent);
            thinkCurrent.reset();
        }

        current = terminal.addMessage("IA", "", true);
    }

    terminal.appendChunk(*current, content);
};

// ============================================================
// SAÍDA DA PESQUISA
// ============================================================

auto beginResearchBlock = [&]()
{
    if (current)
    {
        terminal.finishMessage(*current);
        current.reset();
    }

    if (thinkCurrent)
    {
        terminal.finishMessage(*thinkCurrent);
        thinkCurrent.reset();
    }

    if (researchCurrent)
    {
        terminal.finishResearch(*researchCurrent);
        researchCurrent.reset();
    }

    researchCurrent = terminal.beginResearch();
};

auto appendResearchText = [&](const std::string &content)
{
    if (content.empty())
        return;

    if (!researchCurrent)
        researchCurrent = terminal.beginResearch();

    terminal.appendResearchChunk(*researchCurrent, content);
};

auto finishResearchBlock = [&]()
{
    if (researchCurrent)
    {
        terminal.finishResearch(*researchCurrent);
        researchCurrent.reset();
    }
};

// ============================================================
// IDENTIFICA SUFIXOS QUE PODEM SER PARTE DE UMA TAG
// ============================================================

auto retainedSuffix = [](
    const std::string &buffer,
    std::string_view marker) -> std::size_t
{
    const std::size_t maximum =
        std::min(buffer.size(), marker.size() - 1);

    for (std::size_t length = maximum; length > 0; --length)
    {
        if (buffer.compare(
                buffer.size() - length,
                length,
                marker.data(),
                length) == 0)
        {
            return length;
        }
    }

    return 0;
};

// ============================================================
// PROCESSAMENTO DOS CHUNKS
// ============================================================

auto processOutput = [&](const std::string &chunk)
{
    if (chunk.empty())
        return;

    streamBuffer += chunk;

    while (true)
    {
        // ----------------------------------------------------
        // FORA DO RELATÓRIO
        // ----------------------------------------------------

        if (!insideResearch)
        {
            const std::size_t position =
                streamBuffer.find(openTag);

            if (position != std::string::npos)
            {
                // Tudo antes da abertura pertence à conversa.
                if (position > 0)
                {
                    appendNormalText(
                        streamBuffer.substr(0, position));
                }

                // Remove a tag de abertura.
                streamBuffer.erase(
                    0,
                    position + openTag.size());

                beginResearchBlock();

                insideResearch = true;
                continue;
            }

            // Ainda não encontrou a tag.
            // Preserva somente o possível início de uma tag.
            const std::size_t keep =
                retainedSuffix(streamBuffer, openTag);

            const std::size_t emit =
                streamBuffer.size() - keep;

            if (emit > 0)
            {
                appendNormalText(
                    streamBuffer.substr(0, emit));

                streamBuffer.erase(0, emit);
            }

            break;
        }

        // ----------------------------------------------------
        // DENTRO DO RELATÓRIO
        // ----------------------------------------------------

        const std::size_t position =
            streamBuffer.find(closeTag);

        if (position != std::string::npos)
        {
            // Tudo antes do fechamento pertence ao relatório.
            if (position > 0)
            {
                appendResearchText(
                    streamBuffer.substr(0, position));
            }

            // Remove a tag de fechamento.
            streamBuffer.erase(
                0,
                position + closeTag.size());

            finishResearchBlock();

            insideResearch = false;
            continue;
        }

        // Ainda não encontrou o fechamento.
        // Preserva somente um possível início de </RESEARCH>.
        const std::size_t keep =
            retainedSuffix(streamBuffer, closeTag);

        const std::size_t emit =
            streamBuffer.size() - keep;

        if (emit > 0)
        {
            appendResearchText(
                streamBuffer.substr(0, emit));

            streamBuffer.erase(0, emit);
        }

        break;
    }
};

// ============================================================
// FINALIZA O BUFFER DE UMA RESPOSTA
// ============================================================

auto flushOutput = [&]()
{
    if (!streamBuffer.empty())
    {
        if (insideResearch)
            appendResearchText(streamBuffer);
        else
            appendNormalText(streamBuffer);

        streamBuffer.clear();
    }

    // Se o modelo não fechou a tag, finalizamos o relatório
    // para não deixar a interface presa em estado de streaming.
    if (insideResearch)
    {
        finishResearchBlock();
        insideResearch = false;
    }
};

// ============================================================
// FINALIZA TODAS AS MENSAGENS
// ============================================================

auto finishOpenMessages = [&]()
{
    flushOutput();

    if (current)
    {
        terminal.finishMessage(*current);
        current.reset();
    }

    if (thinkCurrent)
    {
        terminal.finishMessage(*thinkCurrent);
        thinkCurrent.reset();
    }

    finishResearchBlock();

    terminal.finishToolGroup();
};

// ============================================================
// CALLBACKS
// ============================================================

auto onChunk = [&](const std::string &chunk)
{
    processOutput(chunk);
};

auto onThink = [&](const std::string &chunk)
{
    if (!thinkCurrent)
    {
        if (chunk.find_first_not_of(" \t\r\n") == std::string::npos)
            return;

        if (current)
        {
            terminal.finishMessage(*current);
            current.reset();
        }

        thinkCurrent = terminal.addMessage("Thinking", "", true);
    }

    terminal.appendChunk(*thinkCurrent, chunk);
};

auto onTool = [&](
    const std::string &name,
    const ToolResult &result)
{
    if (current)
    {
        terminal.finishMessage(*current);
        current.reset();
    }

    if (thinkCurrent)
    {
        terminal.finishMessage(*thinkCurrent);
        thinkCurrent.reset();
    }

    terminal.appendToolResult(
        name,
        result.success,
        result.output,
        result.error);
};

        try
        {
            auto [role, thinking, audio, texto] = orquestrador.selectModel(prompt);

            bool isNewChat = (chatID == 0);

            auto result = memory.chatHistory(chatID, "User", texto);
            if (!result.success)
                throw std::runtime_error(result.error);

            chatID = result.chatID;

            terminal.setAudioStatus(audio);

            if (texto.find_first_not_of(" \t\n\r") == std::string::npos)
            {
                terminal.setStatus("");
                orquestrador.release();
                return;
            }

            terminal.setStatus(
                std::string(role == ModelRole::Think ? "Think" : "Agent") +
                (thinking ? " | Thinking ON" : " | Thinking OFF"));

            terminal.startThinking();
            timerStarted = true;

            std::string next = texto;
            std::string response;
            do
            {
                response = orquestrador.run(next, role, thinking, audio, onChunk, onThink, onTool);
                finishOpenMessages();

                // Mensagens enviadas durante a execução entram via drainPending().
                next.clear();
            } while (orquestrador.hasPendingOrRelease());

            memory.chatHistory(chatID, "Assistent", response);

            if (isNewChat)
                memory.generateChatTitle(chatID, texto);

            terminal.stopThinking();
            timerStarted = false;
            terminal.setStatus("");
        }
        catch (const std::exception &e)
        {
            if (timerStarted)
            {
                terminal.stopThinking();
                timerStarted = false;
            }

            const bool hasResponse = current.has_value();

            if (hasResponse)
            {
                terminal.appendChunk(
                    *current,
                    "\n\n**Erro na geração:** " + std::string(e.what()));
            }

            finishOpenMessages();

            if (!hasResponse)
                terminal.addMessage("Erro", e.what());

            terminal.setStatus("Erro na geração");
            orquestrador.release();
        }
        catch (...)
        {
            if (timerStarted)
            {
                terminal.stopThinking();
                timerStarted = false;
            }

            const bool hasResponse = current.has_value();

            if (hasResponse)
            {
                terminal.appendChunk(
                    *current,
                    "\n\n**Erro desconhecido na geração.**");
            }

            finishOpenMessages();

            if (!hasResponse)
                terminal.addMessage("Erro", "Ocorreu um erro desconhecido.");

            terminal.setStatus("Erro na geração");
            orquestrador.release();
        } });
}

void Chat::run()
{
    orquestrador.setSpeechCallback(
        [this](const std::string &phrase)
        {
            terminal.showSpeechPhrase(phrase);
        });

    terminal.run(
        [this](const std::string &prompt)
        {
            sendMessage(prompt);
        });

    if (ai_thread.joinable())
        ai_thread.join();
}