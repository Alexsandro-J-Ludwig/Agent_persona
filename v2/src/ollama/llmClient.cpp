#include "llmClient.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>

Json Motor::generate(
    const Model &model,
    const std::string &prompt,
    bool thinking = false)
{
}

Json Motor::chat(
    const Model &model,
    const std::vector<json> &history,
    const std::string &prompt,
    bool thinking = false)
{
}

Json Motor::chatStream(
    const Model &model,
    const std::vector<json> &history,
    StreamCallback callback,
    bool thinking = false,
    const json &tools)
{
}