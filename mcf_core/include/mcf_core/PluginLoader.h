/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_PLUGIN_LOADER_H
#define MCF_PLUGIN_LOADER_H

#include "mcf_core/Plugin.h"

#include <string>
#include <vector>
#include <iostream>

namespace mcf
{
class PluginLoader
{

public:
    /**
     * @brief Constructor
     *
     * Initializes the plugin loading mechanism.
     */
    explicit PluginLoader();

    /**
     * @brief Loads a plugin from a shared object into program memory. The shared object is retained
     * in memory until the destruction of this object.
     *
     * @param fileName The name of the shared object file to load the plugin from.
     * @return A Plugin object that contains component factories.
     */
    Plugin load(const std::string& fileName);

    /**
     * @brief Destroy the Plugin Loader object
     *
     * Unloads all loaded shared objects
     */
    ~PluginLoader();

private:

    struct PluginDescriptor
    {
        /// Path to plugin file
        std::string filePath;

        /// DSO handle
        void* sharedObjectHandle = nullptr;

        /// Plugin object
        Plugin plugin;
    };

    /// Plugin information
    std::vector<PluginDescriptor> _pluginDescs;

    static void unloadHandle(void* handle);

};
} // namespace mcf

#endif // MCF_PLUGIN_LOADER_H