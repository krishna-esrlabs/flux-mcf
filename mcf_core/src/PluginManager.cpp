/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/PluginManager.h"

#include "mcf_core/LoggingMacros.h"

namespace mcf
{
PluginManager::PluginManager(ComponentInstantiator& componentInstantiator)
: _componentInstantiator(componentInstantiator)
{
}

std::vector<std::string>
PluginManager::registerPlugin(const Plugin& plugin)
{
    // check if plugins have unique names?
    auto it = std::find_if(_plugins.begin(), _plugins.end(), [&plugin](const auto& p) {
        return p.name() == plugin.name();
    });
    if (it != _plugins.end())
    {
        MCF_ERROR_NOFILELINE("A plugin with name {} is already registered", plugin.name());
        throw PluginError("Plugins must have unique names");
    }

    std::vector<std::string> typeNames;

    for (const auto& type : plugin.types())
    {
        _componentInstantiator.addComponentType(type);
        typeNames.push_back(type.qualifiedName());
    }

    _componentTypes.insert(std::make_pair(plugin.name(), typeNames));
    _plugins.push_back(plugin);

    return typeNames;
}

std::vector<ComponentProxy>
PluginManager::reloadPlugin(const std::string& pluginName)
{
    auto plugin = std::find_if(_plugins.begin(), _plugins.end(), [&pluginName](const auto& p) {
        return p.name() == pluginName;
    });
    if (plugin == _plugins.end())
    {
        throw PluginError("Plugin not loaded");
    }
    const auto& types = plugin->types();
    auto proxies      = std::vector<ComponentProxy>();

    // for each loaded component: check if it has a type name coming from the plugin and reload it
    // then
    const auto components = _componentInstantiator.listComponents();
    for (const auto& c : components)
    {
        auto it = std::find_if(types.begin(), types.end(), [&c](const ComponentType& type) {
            return type.qualifiedName() == c.typeName();
        });
        if (it != types.end())
        {
            proxies.push_back(_componentInstantiator.reloadComponent(c.name()));
        }
    }

    return proxies;
}

void
PluginManager::erasePlugin(const std::string& pluginName)
{
    auto it = std::find_if(_plugins.begin(), _plugins.end(), [&pluginName](const auto& p) {
        return p.name() == pluginName;
    });
    if (it == _plugins.end())
    {
        throw PluginError("Plugin does not exist!");
    }
    const auto& types = _componentTypes[pluginName];

    for (const auto& t : types)
    {
        _componentInstantiator.removeComponentType(t);
    }

    _plugins.erase(it);
    _componentTypes.erase(pluginName);
}

std::vector<ComponentType>
PluginManager::getComponentTypes(const std::string& pluginName) const
{
    auto it = std::find_if(_plugins.begin(), _plugins.end(), [&pluginName](const Plugin& plugin) {
        return pluginName == plugin.name();
    });
    if (it == _plugins.end())
    {
        throw PluginError("Plugin does not exist");
    }

    return it->types();
}

std::vector<std::string>
PluginManager::getPlugins() const
{
    std::vector<std::string> pluginNames;
    for (const auto& plugin: _plugins)
    {
        pluginNames.push_back(plugin.name());
    }
    return pluginNames;
}

} // namespace mcf