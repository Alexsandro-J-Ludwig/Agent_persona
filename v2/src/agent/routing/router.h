#ifndef ROUTER_H
#define ROUTER_H

#include <tuple>
#include <vector>
#include <json.hpp>
#include <algorithm>
#include <cctype>
#include <iostream>

#include "src/configs/models.h"
#include "src/agent/internal_tools/internalTools.h"

struct RoutingDecision
{
    ModelRole model;
    bool thinking;
    bool audio = false;
};

struct SearchTerm
{
    std::string term;
    int weight;
};

class Router
{
public:
    explicit Router(InternalTools &tool);
    using json = nlohmann::json;

    std::tuple<ModelRole, bool, bool, std::string> route(const std::string &prompt);

private:
    void applyHeuristic(const std::string &prompt);
    void decideThink(const std::string &prompt);
    std::pair<bool, std::string> routeDecide(const std::string &prompt);

    RoutingDecision agent;
    InternalTools &tool;

    std::vector<SearchTerm> busca = {
        // =====================================================
        // RACIOCÍNIO E ANÁLISE
        // =====================================================
        {"analise", 2},
        {"análise", 2},
        {"analisar", 2},
        {"raciocine", 3},
        {"raciocínio", 2},
        {"deduza", 3},
        {"dedução", 3},
        {"infira", 3},
        {"inferência", 2},
        {"demonstre", 3},
        {"demonstração", 3},
        {"prove", 3},
        {"comprove", 3},
        {"justifique", 2},
        {"explique detalhadamente", 2},
        {"passo a passo", 2},
        {"avalie criticamente", 3},
        {"compare", 1},
        {"comparação", 1},
        {"interprete", 2},
        {"investigue", 3},
        {"investigação", 2},
        {"hipótese", 2},
        {"hipóteses", 2},
        {"contradição", 3},
        {"consistência", 2},
        {"causalidade", 3},
        {"formalmente", 3},

        // =====================================================
        // MATEMÁTICA
        // =====================================================
        {"calcule", 1},
        {"resolva", 1},
        {"equação", 1},
        {"sistema de equações", 2},
        {"inequação", 2},
        {"função", 1},
        {"limite", 2},
        {"derivada", 2},
        {"integral", 2},
        {"integral dupla", 3},
        {"integral tripla", 4},
        {"equação diferencial", 4},
        {"álgebra linear", 2},
        {"matriz", 1},
        {"determinante", 2},
        {"autovalor", 3},
        {"autovetor", 3},
        {"espaço vetorial", 3},
        {"transformação linear", 3},
        {"teorema", 3},
        {"prova matemática", 4},
        {"otimização matemática", 4},
        {"análise numérica", 3},
        {"probabilidade", 2},
        {"estatística", 2},
        {"distribuição", 1},
        {"gradiente", 2},
        {"jacobiano", 3},
        {"hessiana", 3},
        {"tensor", 3},

        // =====================================================
        // PROGRAMAÇÃO
        // =====================================================
        {"código", 1},
        {"programação", 1},
        {"implemente", 2},
        {"implementação", 2},
        {"corrija", 2},
        {"corrigir", 2},
        {"debug", 2},
        {"depuração", 2},
        {"erro de compilação", 2},
        {"segmentation fault", 3},
        {"memory leak", 3},
        {"vazamento de memória", 3},
        {"race condition", 4},
        {"deadlock", 4},
        {"concorrência", 3},
        {"multithreading", 3},
        {"assíncrono", 2},
        {"algoritmo", 2},
        {"estrutura de dados", 2},
        {"complexidade computacional", 3},
        {"big o", 2},
        {"recursão", 2},
        {"ponteiro", 2},
        {"gerenciamento de memória", 3},
        {"refatore", 3},
        {"refatoração", 3},
        {"otimize", 3},
        {"otimização", 3},
        {"performance", 2},
        {"benchmark", 2},
        {"profiling", 3},
        {"arquitetura", 3},
        {"arquitetura de software", 4},
        {"design pattern", 3},
        {"microserviços", 3},
        {"sistema distribuído", 4},
        {"escalabilidade", 3},
        {"alta disponibilidade", 4},
        {"tolerância a falhas", 4},
        {"engenharia reversa", 4},

        // =====================================================
        // INTELIGÊNCIA ARTIFICIAL
        // =====================================================
        {"inteligência artificial", 2},
        {"machine learning", 3},
        {"deep learning", 3},
        {"rede neural", 3},
        {"transformer", 3},
        {"attention", 3},
        {"mixture of experts", 4},
        {"moe", 2},
        {"fine tuning", 3},
        {"treinamento", 2},
        {"quantização", 3},
        {"embedding", 2},
        {"rag", 3},
        {"agente autônomo", 4},
        {"tool calling", 3},
        {"context window", 2},
        {"kv cache", 3},
        {"cuda", 2},
        {"rocm", 2},
        {"gpu", 1},
        {"otimização de inferência", 4},

        // =====================================================
        // ENGENHARIA E CIÊNCIAS
        // =====================================================
        {"engenharia", 2},
        {"mecatrônica", 3},
        {"robótica", 3},
        {"cinemática", 3},
        {"cinemática inversa", 4},
        {"dinâmica", 2},
        {"controle", 2},
        {"controlador pid", 3},
        {"sistema de controle", 3},
        {"modelagem matemática", 4},
        {"simulação", 3},
        {"física quântica", 4},
        {"relatividade geral", 4},
        {"termodinâmica", 3},
        {"eletromagnetismo", 3},
        {"circuito elétrico", 2},
        {"análise de circuitos", 3},
        {"processamento de sinais", 4},
        {"transformada de fourier", 3},
        {"transformada de laplace", 3},

        // =====================================================
        // PLANEJAMENTO E PROJETOS
        // =====================================================
        {"planeje", 2},
        {"planejamento", 2},
        {"estratégia", 2},
        {"estratégico", 2},
        {"avalie riscos", 3},
        {"análise de riscos", 3},
        {"viabilidade", 3},
        {"trade-off", 3},
        {"requisitos", 2},
        {"especificação técnica", 3},
        {"documentação técnica", 2},
        {"projeto completo", 4},
        {"desenvolva um sistema", 4},
        {"projete uma arquitetura", 4},
        {"solução escalável", 4},
        {"plano de implementação", 3},
        {"pesquise profundamente", 4},
        {"análise aprofundada", 4},

        // =====================================================
        // COMPLEXIDADE EXPLÍCITA
        // =====================================================
        {"problema complexo", 4},
        {"raciocínio profundo", 5},
        {"análise profunda", 5},
        {"prova formal", 5},
        {"solução completa", 3},
        {"múltiplas etapas", 3},
        {"diversas restrições", 3},
        {"análise multidimensional", 4},
        // =====================================================
        // RACIOCÍNIO PROFUNDO — THINK ON
        // =====================================================

        // Demonstrações matemáticas
        {"demonstre formalmente", 7},
        {"prove rigorosamente", 7},
        {"demonstração matemática", 6},
        {"demonstração rigorosa", 7},
        {"derive matematicamente", 7},
        {"prove um teorema", 7},
        {"deduza a expressão", 6},
        {"demonstre por indução", 7},
        {"demonstre por contradição", 7},
        {"encontre uma prova", 7},

        // Problemas matemáticos complexos
        {"equações diferenciais parciais", 7},
        {"otimização não linear", 6},
        {"otimização multiobjetivo", 7},
        {"problema de otimização com restrições", 7},
        {"análise de estabilidade", 6},
        {"resolva analiticamente", 6},
        {"derive as equações", 7},

        // Programação avançada
        {"investigue um deadlock", 7},
        {"identifique uma race condition", 7},
        {"analise concorrência distribuída", 7},
        {"projete um sistema distribuído", 7},
        {"projete uma arquitetura escalável", 7},
        {"otimize a complexidade algorítmica", 7},
        {"investigue um vazamento de memória", 6},
        {"refatore uma arquitetura completa", 7},
        {"desenvolva um compilador", 8},

        // Engenharia e física
        {"derive as equações de movimento", 7},
        {"modele matematicamente", 6},
        {"projete um controlador", 6},
        {"analise a estabilidade do sistema", 7},
        {"resolva um problema de cinemática inversa", 7},
        {"derive as equações de campo", 8},
        {"simule um sistema dinâmico", 6},

        // Investigação e raciocínio
        {"investigue profundamente", 7},
        {"analise todas as possibilidades", 7},
        {"avalie hipóteses concorrentes", 7},
        {"identifique a causa raiz", 6},
        {"resolva um problema complexo", 6},
        {"desenvolva uma solução completa", 6},
        {"avalie cenários contraditórios", 7},
        {"raciocine profundamente", 8},
        {"análise multidisciplinar", 7},
        {"demonstre a impossibilidade", 8}};
};

#endif