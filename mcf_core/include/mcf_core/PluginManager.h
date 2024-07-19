/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_PLGUN_MANAGER_H
#define MCF_PLGUN_MANAGER_H

#include "mcf_core/ComponentInstantiator.h"
#include "mcf_core/Plugin.h"

namespace mcf
{
/**
 * @brief A plugin management class
 *
 * Manages the plugin lifecycle (loading and unloading).
 */
class PluginManager
{
private:
    ComponentInstantiator& _componentInstantiator;

    std::vector<Plugin> _plugins;
    std::map<std::string, std::vector<std::string> > _componentTypes;

public:
    /**
     * @brief Construct a new Plugin Manager object
     * 
     * @param componentInstantiator The component instantiator object with which the types from the plugin should be registered.
     */
    explicit PluginManager(ComponentInstantiator& componentInstantiator);

    /**
     * @brief Registers a new plugin
     * 
     * @param plugin The plugin to register
     * @return The list of qualified type names that the plugin contains.
     */
    std::vector<std::string> registerPlugin(const Plugin& plugin);

    /**
     * @brief Reloads a plugin, reloading all components that have been instantiated from it
     * 
     * @param pluginName 
     * @return std::vector<ComponentProxy> 
     */
    std::vector<ComponentProxy> reloadPlugin(const std::string& pluginName);

    /**
     * @brief Removes the plugin and all components that have been instantiated in it.
     * 
     * @param pluginName The name of the plugin
     */
    void erasePlugin(const std::string& pluginName);

    std::vector<ComponentType> getComponentTypes(const std::string& pluginName) const;
    std::vector<std::string> getPlugins() const;
};
} // namespace mcf

#endif // MCF_PLGUN_MANAGER_H