#include "agentOrchestrator.h"
#include <stdexcept>

Orquestrador::Orquestrador()
    : router(tools),
      manager(tools),
      speech([this](const std::string &phrase)
             {
          const auto result =
              tools.executeSystemTool("audioPlayer", phrase);

          if (!result.success)
              throw std::runtime_error(result.error); })
{
}

std::tuple<ModelRole, bool, bool, std::string> Orquestrador::selectModel(
    const std::string &prompt)
{
    const auto result = router.route(prompt);

    setAudioEnabled(std::get<2>(result));

    return {std::get<0>(result), std::get<1>(result), std::get<2>(result), std::get<3>(result)};
}

std::string Orquestrador::run(
    const std::string &prompt,
    ModelRole role,
    bool thinking,
    bool audio,
    Ollama::StreamCallback onChunk)
{
    const Model &model = getModel(role);

    // Altera o system prompt com base no modelo selecionado em router
    if (manager.getHistory().empty())
        manager.createHistory(role, thinking);
    else
        manager.alterSystemHistory(role, thinking);

    // Adiciona o prompt do usuario no historico
    manager.addHistory("user", prompt);

    std::string pendingSpeech;

    std::string response = ollama.chatStream(
        model,
        manager.getHistory(),
        [&](const std::string &chunk)
        {
            onChunk(chunk); // Atualiza o terminal.

            if (audio)
            {
                pendingSpeech += chunk;

                bool hasEndPunct = (pendingSpeech.find('.') != std::string::npos ||
                                    pendingSpeech.find('!') != std::string::npos ||
                                    pendingSpeech.find('?') != std::string::npos ||
                                    pendingSpeech.find('\n') != std::string::npos ||
                                    pendingSpeech.find(", ") != std::string::npos);

                // Se a frase já passou de 45 caracteres e tem uma vírgula ou ':', já envia pro TTS!
                bool hasLongClause = (pendingSpeech.size() >= 45 &&
                                      (pendingSpeech.find(',') != std::string::npos ||
                                       pendingSpeech.find(':') != std::string::npos ||
                                       pendingSpeech.find(';') != std::string::npos));

                if (hasEndPunct || hasLongClause)
                {
                    if (pendingSpeech.find_first_not_of(" \t\r\n") != std::string::npos)
                        speech.enqueue(pendingSpeech);
                    pendingSpeech.clear();
                }
            }
        },
        thinking);

    if (audio && !pendingSpeech.empty())

        speech.enqueue(pendingSpeech);

    // Adiciona a resposta da IA no historico
    manager.addHistory("assistant", response);

    return response;
}

void Orquestrador::setAudioEnabled(bool enabled)
{
    if (!enabled)
    {
        speech.cancelPending();
        speech.wait();
    }
}
