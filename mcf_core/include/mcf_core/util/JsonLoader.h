/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_UTIL_JSONLOADER_H_
#define MCF_UTIL_JSONLOADER_H_

#include "json/json.h"

#include <string>

namespace mcf
{
namespace util
{
namespace json
{
Json::Value loadFile(const std::string& filename);
}
} // namespace util
} // namespace mcf

#endif // MCF_UTIL_JSONLOADER_H_
