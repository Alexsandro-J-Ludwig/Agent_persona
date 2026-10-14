#ifndef ROUTER_H
#define ROUTER_H

#include <tuple>
#include <vector>

#include "src/configs/models.h"

struct RoutingDecision
{
    ModelRole model;
    bool thinking;
};

struct SearchTerm
{
    std::string term;
    int weight;
};

class Router
{
public:
    std::tuple<ModelRole, bool> route(const std::string &prompt);

private:
    void forceThinking(const std::string &prompt);
    std::pair<bool, std::string>  routeDecide(const std::string &prompt);

    RoutingDecision agent;

    std::vector<SearchTerm> busca = {
        // Análise e raciocínio
        {"analise", 2},
        {"análise", 2},
        {"raciocine", 3},
        {"deduza", 3},
        {"demonstre", 3},
        {"prove", 3},

        // Matemática
        {"calcule", 1},
        {"equação", 1},
        {"integral", 2},
        {"teorema", 3},
        {"demonstração", 3},

        // Programação
        {"código", 1},
        {"corrija", 2},
        {"debug", 2},
        {"algoritmo", 2},
        {"arquitetura", 3},
        {"otimização", 3},

        // Tarefas complexas
        {"planeje", 2},
        {"investigue", 3},
        {"pesquise profundamente", 4},
        {"desenvolva um sistema", 4},
        {"projete uma arquitetura", 4},
        {"demonstre formalmente", 6},
    };

};

#endif