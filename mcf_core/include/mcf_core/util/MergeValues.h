/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_UTIL_MERGEVALUES_H
#define MCF_UTIL_MERGEVALUES_H

#include "json/forwards.h"

#include <vector>

namespace mcf
{
namespace util
{
namespace json
{

/**
 * Update Json value "parent" with contents of value "child"
 */
void updateValue(Json::Value& parent, const Json::Value& child);

/**
 * Merge two Json values, so that value "parent" is updated/extended with contents of value "child"
 */
Json::Value mergeValues(const Json::Value& parent, const Json::Value& child);

/**
 * Merge Json parent value with multiple children in the given sequence
 */
Json::Value mergeValues(const Json::Value& parent, const std::vector<Json::Value>& children);

/**
 * Create value from multiple Json files merged in the given order
 */
Json::Value mergeFiles(const std::vector<std::string>& filePaths, bool skipMissing = false);

} // namespace json
} // namespace util
} // namespace mcf

#endif // MCF_UTIL_MERGEVALUES_H
