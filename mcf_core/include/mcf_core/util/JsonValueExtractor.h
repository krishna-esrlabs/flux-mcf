/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_UTIL_JSONVALUEEXTRACTOR_H_
#define MCF_UTIL_JSONVALUEEXTRACTOR_H_

#include "json/json.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace mcf
{
namespace util
{
namespace json
{

class JsonValueExtractor
{
public:
    JsonValueExtractor(std::string id);

    /**
     * @brief Extract a single value from a config object
     *
     * This template function is specialized for the following types:
     * bool, int, float, std::string, std::vector<int>, std::vector<float>,
     * std::vector<std::string>, std::vector<std::vector<int>>, std::vector<std::vector<float>>,
     * std::set<std::string>
     *
     * The method throws if the object is not of the desired type
     *
     * @tparam T The type of the value
     * @param config The configuration object
     * @param error The description of the object in a possible error string
     * @return T The value, if it has the correct type
     */
    template <typename T>
    T extractConfigValue(const Json::Value& config, const std::string& error) const;

    /**
     * @brief Convenience wrapper to extract a value from a configuration object by name
     *
     * @tparam T Type of the extracted value
     * @param object Configuration object
     * @param member Member name of the configuration object
     * @return T Extracted value
     */
    template <typename T>
    T extractConfigMember(const Json::Value& object, const std::string& member) const
    {
        return extractConfigValue<T>(object[member], member);
    }

    /**
     * @brief Convenience wrapper that extracts a value from a config object if it is not null or
     * else returns a default value
     *
     * @tparam T Type of the return value
     * @param object JSON configuration object
     * @param member Object member name that has to be extracted
     * @param defaultValue Default value
     * @return T Either `object[member]`, or, if this is null or absent, the default value
     */
    template <typename T>
    T extractConfigMember(
        const Json::Value& object, const std::string& member, const T& defaultValue) const
    {
        if (object[member].isNull())
        {
            return defaultValue;
        }
        return extractConfigMember<T>(object, member);
    }

    /**
     * Extract single float from json config object
     */
    float extractConfigFloat(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract single double from json config object
     */
    double extractConfigDouble(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract single int from json config object
     */
    int extractConfigInt(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract single unsigned int from json config object
     */
    unsigned int extractConfigUInt(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract single bool from json config object
     */
    bool extractConfigBool(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract single string from json config object
     */
    std::string extractConfigString(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract vector of strings from json config object
     */
    std::vector<std::string>
    extractConfigStringVector(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract vector of floats from json config object
     */
    std::vector<float>
    extractConfigFloatVector(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract vector of doubles from json config object
     */
    std::vector<double>
    extractConfigDoubleVector(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract vector of ints from json config object
     */
    std::vector<int>
    extractConfigIntVector(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract vector of unsigned ints from json config object
     */
    std::vector<unsigned int>
    extractConfigUIntVector(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract 2d vector of ints from json config object
     */
    std::vector<std::vector<int> >
    extractConfigIntVector2d(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract 2d vector of floats from json config object
     */
    std::vector<std::vector<float> >
    extractConfigFloatVector2d(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract set of strings from json config object
     */
    std::set<std::string>
    extractConfigStringSet(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract string to int map from json config object
     */
    std::map<std::string, int>
    extractConfigStrIntMap(const Json::Value& config, const std::string& errorValue) const;

    /**
     * Extract string to string map from json config object
     */
    std::map<std::string, std::string>
    extractConfigStrMap(const Json::Value& config, const std::string& errorValue) const;

private:
    std::string fId;

    std::string makeErrorString(const std::string& errorValue) const
    {
        return "Missing or invalid " + errorValue + " in " + fId + " configuration";
    }
};

template <>
inline float
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigFloat(config, error);
}

template <>
inline int
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigInt(config, error);
}

template <>
inline unsigned int
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigUInt(config, error);
}

template <>
inline bool
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigBool(config, error);
}

template <>
inline std::string
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigString(config, error);
}

template <>
inline std::vector<float>
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigFloatVector(config, error);
}

template <>
inline std::vector<double>
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigDoubleVector(config, error);
}

template <>
inline std::vector<std::vector<float> >
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigFloatVector2d(config, error);
}

template <>
inline std::vector<int>
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigIntVector(config, error);
}

template <>
inline std::vector<unsigned int>
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigUIntVector(config, error);
}

template <>
inline std::vector<std::vector<int> >
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigIntVector2d(config, error);
}

template <>
inline std::vector<std::string>
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigStringVector(config, error);
}

template <>
inline std::set<std::string>
JsonValueExtractor::extractConfigValue(const Json::Value& config, const std::string& error) const
{
    return extractConfigStringSet(config, error);
}

} // namespace json
} // namespace util
} // namespace mcf

#endif // MCF_UTIL_JSONVALUEEXTRACTOR_H_
