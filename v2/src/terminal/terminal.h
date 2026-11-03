#ifndef TERMINAL_H
#define TERMINAL_H

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "src/markdown/markdownRenderer.h"
#include "src/markdown/markdownFtxui.h"

using MessageId = std::uint64_t;

struct ChatMessage
{
    MessageId id{};
    std::string role;
    std::string message;
    std::string label;
    MarkdownRenderer markdown;
    bool finished = false;
};

class TerminalDisplay
{
public:
    using SubmitCallback = std::function<void(const std::string &)>;

    // Ciclo de vida da interface.
    void run(SubmitCallback onSubmit);

    // Mensagens.
    MessageId addMessage(
        const std::string &role,
        const std::string &message,
        bool streaming = false,
        std::string label = {});

    void appendChunk(MessageId id, const std::string &chunk);
    void finishMessage(MessageId id);

    // Resultados das ferramentas.
    void appendToolResult(
        const std::string &name,
        bool success,
        const std::string &output,
        const std::string &error);

    void finishToolGroup();

    // Estado visível.
    void setStatus(const std::string &status);
    void setAudioStatus(bool status);
    void showSpeechPhrase(const std::string &phrase);
    void startThinking();
    void setAwaitToken();
    double stopThinking();

    // Elementos reutilizáveis.
    ftxui::Element commandView();
    ftxui::Element spinnerElement();

private:
    // Construção de componentes.
    ftxui::Component initDisplay();
    ftxui::Component displayInput(SubmitCallback onSubmit);
    ftxui::Component thinkingStatus();
    ftxui::Element voiceView();

    // Tela.
    ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();

    // Estado compartilhado entre threads. Acesso protegido por chat_mutex.
    mutable std::mutex chat_mutex;
    std::vector<ChatMessage> messages;
    std::vector<MessageId> pendingUiMessages;
    std::string statusMessage;
    MessageId nextId = 0;
    std::optional<MessageId> toolGroupMessage;
    std::size_t toolCount = 0;

    std::chrono::steady_clock::time_point generationStart{};
    bool awaitToken = false;
    std::deque<std::string> speechCaptions;
    std::chrono::steady_clock::time_point captionStart{};

    // Estado atômico compartilhado entre UI e workers.
    std::atomic<bool> isGenerating{false};
    std::atomic<bool> audioEnabled{false};
    std::atomic<int> spinnerFrame{0};

    std::jthread timerThread;
    std::jthread voiceAnimation;
    ftxui::Box voiceBox{};

    // Estado manipulado exclusivamente pela thread da UI.
    std::string prompt;
    float scrollPosition = 1.0f;
    bool showCommands = false;

};

#endif // TERMINAL_H
