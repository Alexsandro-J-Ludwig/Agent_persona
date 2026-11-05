#ifndef SPEACH_H
#define SPEACH_H

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

class Speach
{
public:
    using Speaker = std::function<void(const std::string &)>;
    using Stopper = std::function<void()>;

    explicit Speach(Speaker speaker, Stopper stopper);
    ~Speach();

    Speach(const Speach &) = delete;
    Speach &operator=(const Speach &) = delete;

    // Fila de fala: apenas coloca a frase na fila, sem esperar reprodução.
    void enqueue(std::string phrase);
    void wait();

    // Alimentação por streaming.
    void feed(const std::string &chunk);
    void flush();

    // Controle de interrupção.
    void cancelPending(); // limpa fila e corta o áudio atual
    void interrupt();     // igual ao anterior, e silencia o resto da resposta
    void resume();        // volta a aceitar fala

    // Estado e observadores.
    std::string lastError() const;
    void setPhraseCallback(Speaker callback);

private:
    void consume();
    std::string audioCleaner(const std::string &text, bool final = false);

    // Callbacks.
    Speaker speaker;
    Stopper stopper;
    Speaker phraseCallback; // protegido por mtx

    // Estado do alimentador e do limpador de markdown (protegido por feedMtx).
    std::mutex feedMtx;
    bool muted = false;
    bool insideCodeBlock = false;
    bool atLineStart = true;
    char fenceMarker = '`';
    std::size_t fenceLength = 3;
    std::string markdownBuffer;
    std::string pendingSpeech;

    // Fila e worker (protegido por mtx).
    // Ordem dos locks: feedMtx primeiro, depois mtx.
    mutable std::mutex mtx;
    std::condition_variable ready;
    std::condition_variable idle;
    std::deque<std::string> phrases;
    std::string error;
    unsigned long generation = 0;
    bool stopping = false;
    bool active = false;
    std::thread worker; // declarado por último: inicia após os demais membros
};

#endif