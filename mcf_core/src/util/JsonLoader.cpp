/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/util/JsonLoader.h"

#include <fstream>
#include <iostream>

namespace mcf
{
namespace util
{
namespace json
{

Json::Value
loadFile(const std::string& filename)
{
    Json::Value jsonValue;

    std::ifstream fileStream(filename);
    if (!fileStream)
    {
        std::cout << "loadConfig: could not open file " << filename << std::endl;
        return jsonValue;
    }
    fileStream >> jsonValue;
    return jsonValue;
}
} // namespace json
} // namespace util
} // namespace mcf