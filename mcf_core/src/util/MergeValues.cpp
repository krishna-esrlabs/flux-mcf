/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/util/MergeValues.h"

#include "json/json.h"

#include <fstream>
#include <string>

namespace mcf
{
namespace util
{
namespace json
{

/*
 * Update value "parent" cith contents value "child"
 */
void
updateValue(Json::Value& parent, const Json::Value& child)
{
    // we will update only if both values are objects
    if (parent.isObject() && child.isObject())
    {
        // loop over all members of child
        for (const auto& name : child.getMemberNames())
        {
            try
            {
                // get corresponding parent avd child entries (or null)
                Json::Value& parentEntry      = parent[name];
                const Json::Value& childEntry = child[name];

                // recursively update parent value, if it is an object
                if (parentEntry.isObject())
                {
                    updateValue(parentEntry, childEntry);
                }

                // otherwise replace parent entry or create new parent entry
                else
                {
                    parentEntry = childEntry;
                }
            }
            catch (const Json::Exception& e)
            {
                throw Json::RuntimeError("Cannot merge entry '" + name + "': " + e.what());
            }
        }
    }

    else
    {
        throw Json::RuntimeError("Parent or child is not a Json object");
    }
}

/*
 * Merge two json values, so that value "parent" is updated/extended with contents of value "child"
 */
Json::Value
mergeValues(const Json::Value& parent, const Json::Value& child)
{
    Json::Value parentCopy = parent;
    updateValue(parentCopy, child);
    return parentCopy;
}
/*
 * Merge Json parent value with multiple children in the given sequence
 */
Json::Value
mergeValues(const Json::Value& parent, const std::vector<Json::Value>& children)
{
    Json::Value parentCopy = parent;
    for (size_t i = 0; i < children.size(); ++i)
    {
        try
        {
            updateValue(parentCopy, children[i]);
        }
        catch (const Json::Exception& e)
        {
            throw Json::RuntimeError("Cannot merge child #" + std::to_string(i));
        }
    }

    return parentCopy;
}

/*
 * Create value from multiple Json files merged in the given order
 */
Json::Value
mergeFiles(const std::vector<std::string>& filePaths, bool skipMissing)
{
    // create empty json object value to receive the merged config
    Json::Value merged(Json::ValueType::objectValue);

    // loop over all config files
    for (const auto& filePath : filePaths)
    {
        // open config file, abort or skip, if not available
        std::ifstream fileStream(filePath);
        if (!fileStream)
        {
            if (!skipMissing)
            {
                throw Json::RuntimeError("Failed to open Json file " + filePath);
            }
            else
            {
                continue;
            }
        }

        // read config
        Json::Value curConfig;
        fileStream >> curConfig;

        // merge with read before
        try
        {
            mcf::util::json::updateValue(merged, curConfig);
        }
        catch (const Json::Exception& e)
        {
            throw Json::RuntimeError("Failed to merge Json file " + filePath + ": " + e.what());
        }
    }
    return merged;
}

} // namespace json
} // namespace util
} // namespace mcf
