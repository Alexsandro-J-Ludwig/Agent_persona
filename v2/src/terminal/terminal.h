#ifndef TERMINAL_H
#define TERMINAL_H

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "src/markdown/markdownRenderer.h"
#include "src/markdown/markdownFtxui.h"

using MessageId = std::uint64_t;

struct ChatMessage
{
    MessageId id;
    std::string role;
    std::string message;
    MarkdownRenderer markdown;
};

class TerminalDisplay
{
public:
    using SubmitCallback = std::function<void(const std::string &)>;

    // ---- Ciclo de vida ----
    void run(SubmitCallback onSubmit);

    // ---- Mensagens ----
    MessageId addMessage(const std::string &role, const std::string &message);
    void appendChunk(MessageId id, const std::string &chunk);

    // ---- Estado visível (status, áudio, "pensando") ----
    void setStatus(const std::string &status);
    void setAudioStatus(bool status);
    // ====== LEGENDA POR FRASE ENVIADA AO REPRODUTOR ======
    void showSpeechPhrase(const std::string &phrase);
    void startThinking();
    void setAwaitToken();
    double stopThinking();

    // ---- Elementos reutilizáveis ----
    ftxui::Element commandView();
    ftxui::Element spinnerElement();

private:
    // ---- Construção da UI ----
    ftxui::Component initDisplay();
    ftxui::Component displayInput(SubmitCallback onSubmit);
    ftxui::Component thinkingStatus();
    ftxui::Element historyView();
    // ====== PERSPECTIVA DE VOZ: CANVAS E LEGENDA ======
    ftxui::Element voiceView();

    // ---- Tela ----
    ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();

    // ---- Estado compartilhado entre threads (protegido por chat_mutex) ----
    std::mutex chat_mutex;
    std::vector<ChatMessage> messages;
    std::string statusMessage;
    MessageId nextId = 0;

    // ---- Estado atômico (lido pela UI, escrito por outras threads) ----
    std::atomic<bool> isGenerating{false};
    std::atomic<bool> audioEnabled{false};
    std::atomic<int> spinnerFrame{0};
    std::chrono::steady_clock::time_point generationStart{};
    std::jthread timerThread;
    // ====== ANIMAÇÃO VISUAL DO MODO DE VOZ ======
    std::jthread voiceAnimation;
    ftxui::Box voiceBox;
    std::deque<std::string> speechCaptions;
    std::string lastCaption;
    std::chrono::steady_clock::time_point captionStart{};


    // ---- Estado só da UI ----
    std::string prompt;
    float scrollPosition = 1.0f;
    bool showCommands = false;
    bool awaitToken = false;

    // ---- Renderização ----
    MarkdownRenderer render;
};

#endif
