#include "src/agent/orchestrator/speach/speach.h"

#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std::chrono_literals;

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

// Libera o callback antes de destruir Speach, inclusive se um teste falhar.
struct ReleaseOnExit
{
    std::promise<void> &promise;
    bool released = false;
    void open()
    {
        if (!released)
        {
            promise.set_value();
            released = true;
        }
    }
    ~ReleaseOnExit() { open(); }
};

void testAsyncAndOrder()
{
    std::promise<void> entered, release;
    auto enteredFuture = entered.get_future();
    auto released = release.get_future().share();
    std::vector<std::string> spoken;
    std::thread::id workerId;
    Speach speech([&](const std::string &phrase)
                  {
                      workerId = std::this_thread::get_id();
                      if (phrase == "Olá!") {
                          entered.set_value();
                          released.wait();
                      }
                      spoken.push_back(phrase); });
    ReleaseOnExit gate{release};
    speech.enqueue("Olá!");
    require(enteredFuture.wait_for(5s) == std::future_status::ready,
            "Worker não iniciou");

    auto producer = std::async(std::launch::async, [&]
                               {
                                   speech.enqueue("Como posso ajudar?");
                                   speech.enqueue("Trecho final sem pontuação"); });
    const bool nonBlocking = producer.wait_for(1s) == std::future_status::ready;
    gate.open();
    producer.get();
    speech.wait();
    require(nonBlocking, "enqueue esperou o áudio terminar");
    require(workerId != std::this_thread::get_id(), "Áudio executou na thread principal");
    require(spoken == std::vector<std::string>{"Olá!", "Como posso ajudar?", "Trecho final sem pontuação"},
            "Frases fora de ordem ou perdidas");
}

void testFailureAndEmptyInput()
{
    std::vector<std::string> spoken;
    Speach speech([&](const std::string &phrase)
                  {
                      if (phrase == "falha") throw std::runtime_error("TTS indisponível");
                      spoken.push_back(phrase); });
    speech.enqueue(" \t\n");
    speech.enqueue("falha");
    speech.enqueue("seguinte");
    speech.wait();
    require(spoken == std::vector<std::string>{"seguinte"}, "Worker parou após falha ou aceitou vazio");
    require(speech.lastError() == "TTS indisponível", "Falha não foi registrada");
}

void testCancelPending()
{
    std::promise<void> entered, release;
    auto enteredFuture = entered.get_future();
    auto released = release.get_future().share();
    std::vector<std::string> spoken;
    Speach speech([&](const std::string &phrase)
                  {
                      entered.set_value();
                      released.wait();
                      spoken.push_back(phrase); });
    ReleaseOnExit gate{release};
    speech.enqueue("em andamento");
    require(enteredFuture.wait_for(5s) == std::future_status::ready, "Worker não iniciou");
    speech.enqueue("pendente");
    speech.cancelPending();
    gate.open();
    speech.wait();
    require(spoken == std::vector<std::string>{"em andamento"}, "Cancelamento não limpou a fila");
    require(speech.lastError().empty(), "Worker tentou executar uma frase cancelada");
}

void testShutdownDrainsQueue()
{
    std::vector<std::string> spoken;
    {
        Speach speech([&](const std::string &phrase)
                      { spoken.push_back(phrase); });
        speech.enqueue("primeira");
        speech.enqueue("segunda");
    }
    require(spoken == std::vector<std::string>{"primeira", "segunda"},
            "Destrutor abandonou frases ou não aguardou o worker");
}

int main()
{
    try
    {
        testAsyncAndOrder();
        testFailureAndEmptyInput();
        testCancelPending();
        testShutdownDrainsQueue();
        std::cout << "PASS: enqueue não bloqueante, FIFO, thread separada, falhas, cancelamento e encerramento.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
