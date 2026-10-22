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

    explicit Speach(Speaker speaker);
    ~Speach();

    Speach(const Speach &) = delete;
    Speach &operator=(const Speach &) = delete;

    // Apenas coloca a frase na fila; não espera geração nem reprodução.
    void enqueue(std::string phrase);
    void wait();
    void cancelPending();
    std::string lastError() const;

private:
    void consume();
    bool insideCodeBlock = false;

    std::string audioCleaner(const std::string &text);

    Speaker speaker;
    mutable std::mutex mtx;
    std::condition_variable ready;
    std::condition_variable idle;
    std::deque<std::string> phrases;
    std::string error;
    bool stopping = false;
    bool active = false;
    std::thread worker;
};

#endif
