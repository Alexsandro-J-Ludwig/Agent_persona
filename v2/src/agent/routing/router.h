#ifndef ROUTER_H
#define ROUTER_H
#include <tuple>

#include "../configs/models.h"

class Router
{
public:
    std::tuple<ModelRole, bool> route(const std::string &prompt);
};

#endif