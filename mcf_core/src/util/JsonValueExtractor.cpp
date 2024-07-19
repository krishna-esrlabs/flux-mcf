/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/util/JsonValueExtractor.h"

#include "mcf_core/ErrorMacros.h"

namespace mcf
{
namespace util
{
namespace json
{

JsonValueExtractor::JsonValueExtractor(std::string id) : fId(std::move(id)) {}

float
JsonValueExtractor::extractConfigFloat(
        const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isNumeric(), makeErrorString(errorValue));
    return config.asFloat();
}

double
JsonValueExtractor::extractConfigDouble(
        const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isNumeric(), makeErrorString(errorValue));
    return config.asDouble();
}

int
JsonValueExtractor::extractConfigInt(const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isInt(), makeErrorString(errorValue));
    return config.asInt();
}

unsigned int
JsonValueExtractor::extractConfigUInt(const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isUInt(), makeErrorString(errorValue));
    return config.asUInt();
}

bool
JsonValueExtractor::extractConfigBool(
    const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isBool(), makeErrorString(errorValue));
    return config.asBool();
}

std::string
JsonValueExtractor::extractConfigString(
    const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isString(), makeErrorString(errorValue));
    return config.asString();
}

std::vector<std::string>
JsonValueExtractor::extractConfigStringVector(
    const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isArray(), makeErrorString(errorValue));

    std::vector<std::string> strVec;
    Json::ArrayIndex len = config.size();
    for (Json::ArrayIndex i = 0; i != len; ++i)
    {
        MCF_ASSERT(config[i].isString(), makeErrorString(errorValue));
        strVec.emplace_back(config[i].asString());
    }
    return strVec;
}

std::vector<float>
JsonValueExtractor::extractConfigFloatVector(
        const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isArray(), makeErrorString(errorValue));

    std::vector<float> vec;
    Json::ArrayIndex len = config.size();
    for (Json::ArrayIndex i = 0; i != len; ++i)
    {
        MCF_ASSERT(config[i].isNumeric(), makeErrorString(errorValue));
        vec.emplace_back(config[i].asFloat());
    }
    return vec;
}

std::vector<double>
JsonValueExtractor::extractConfigDoubleVector(
        const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isArray(), makeErrorString(errorValue));

    std::vector<double> vec;
    Json::ArrayIndex len = config.size();
    for (Json::ArrayIndex i = 0; i != len; ++i)
    {
        MCF_ASSERT(config[i].isNumeric(), makeErrorString(errorValue));
        vec.emplace_back(config[i].asDouble());
    }
    return vec;
}

std::vector<int>
JsonValueExtractor::extractConfigIntVector(
        const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isArray(), makeErrorString(errorValue));

    std::vector<int> vec;
    Json::ArrayIndex len = config.size();
    for (Json::ArrayIndex i = 0; i != len; ++i)
    {
        MCF_ASSERT(config[i].isIntegral(), makeErrorString(errorValue));
        vec.emplace_back(config[i].asInt());
    }
    return vec;
}

std::vector<unsigned int>
JsonValueExtractor::extractConfigUIntVector(
        const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isArray(), makeErrorString(errorValue));

    std::vector<unsigned int> vec;
    Json::ArrayIndex len = config.size();
    for (Json::ArrayIndex i = 0; i != len; ++i)
    {
        MCF_ASSERT(config[i].isUInt(), makeErrorString(errorValue));
        vec.emplace_back(config[i].asInt());
    }
    return vec;
}

std::vector<std::vector<int> >
JsonValueExtractor::extractConfigIntVector2d(
    const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isArray(), makeErrorString(errorValue));
    MCF_ASSERT(config[0].isArray(), makeErrorString(errorValue));

    std::vector<std::vector<int> > vec;
    Json::ArrayIndex ilen = config.size();
    for (Json::ArrayIndex i = 0; i != ilen; ++i)
    {
        vec.emplace_back(std::vector<int>());
        Json::ArrayIndex jlen = config[i].size();
        for (Json::ArrayIndex j = 0; j != jlen; ++j)
        {
            vec[i].emplace_back(config[i][j].asInt());
        }
    }
    return vec;
}

std::vector<std::vector<float> >
JsonValueExtractor::extractConfigFloatVector2d(
    const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isArray() && !config.empty(), makeErrorString(errorValue));
    MCF_ASSERT(config[0].isArray(), makeErrorString(errorValue));

    std::vector<std::vector<float> > vec;
    Json::ArrayIndex ilen = config.size();
    for (Json::ArrayIndex i = 0; i != ilen; ++i)
    {
        vec.emplace_back(std::vector<float>());
        Json::ArrayIndex jlen = config[i].size();
        for (Json::ArrayIndex j = 0; j != jlen; ++j)
        {
            vec[i].emplace_back(config[i][j].asFloat());
        }
    }
    return vec;
}

std::set<std::string>
JsonValueExtractor::extractConfigStringSet(
    const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(config.isArray(), makeErrorString(errorValue));

    std::set<std::string> strSet;
    Json::ArrayIndex len = config.size();
    for (Json::ArrayIndex i = 0; i != len; ++i)
    {
        MCF_ASSERT(config[i].isString(), makeErrorString(errorValue));
        strSet.insert(config[i].asString());
    }
    return strSet;
}

std::map<std::string, int>
JsonValueExtractor::extractConfigStrIntMap(const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(!config.empty(), makeErrorString(errorValue));
    std::map<std::string, int> strIntMap;
    for (auto const& name : config.getMemberNames())
    {
        strIntMap[name] = extractConfigInt(config[name], name);
    }
    return strIntMap;
}

std::map<std::string, std::string>
JsonValueExtractor::extractConfigStrMap(const Json::Value& config, const std::string& errorValue) const
{
    MCF_ASSERT(!config.empty(), makeErrorString(errorValue));
    std::map<std::string, std::string> strIntMap;
    for (auto const& name : config.getMemberNames())
    {
        strIntMap[name] = extractConfigString(config[name], name);
    }
    return strIntMap;
}

} // namespace json
} // namespace util
} // namespace mcf
