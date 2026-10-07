#include "router.h"

std::tuple<ModelRole, bool> Router::route(const std::string &prompt)
{
    return {ModelRole::Agent, false};
};