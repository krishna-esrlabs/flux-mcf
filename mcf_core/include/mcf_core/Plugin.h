/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_PLUGIN_H
#define MCF_PLUGIN_H

#include "mcf_core/ComponentType.h"
#include "mcf_core/IComponent.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mcf
{
/// Entry point for plugin DSOs
static const char* PLUGIN_ENTRY_POINT = "initializePlugin";

/**
 * @brief An exception type for plugin-related errors
 *
 */
class PluginError : public std::runtime_error
{
public:
    PluginError(const std::string& what) : std::runtime_error(what) {}
};

/**
 * @brief A minimalistic plugin interface
 *
 * Consists of a name (in order to refer to it) and a list of component-interface-complying-object
 * factories.
 */
class Plugin
{
private:
    std::vector<ComponentType> _types;
    std::string _name;

public:
    /**
     * @brief Construct a new Plugin object
     *
     * @param name The name of the plugin
     * @param factories The component factories
     */
    Plugin(std::string name, std::vector<ComponentType> types)
    : _name(std::move(name)), _types(std::move(types))
    {
    }

    /**
     * @brief Types accessor method
     *
     * @return The contained component types
     */
    std::vector<ComponentType> types() const { return _types; }

    /**
     * @brief Name accessor
     *
     * @return The plugin name
     */
    std::string name() const { return _name; }
};

/**
 * @brief A minimalistic helper for creation of plugins
 *
 * Use case should be something like
 * `PluginBuilder(name).addFactory<>().addFactory<>().addFactory<>().toPlugin()`. The idea is to use
 * the builder pattern to contain the mutable behaviour in the builder class and construct an
 * immutable plugin class from it.
 *
 * This class is intended to separate a mutable, append-only builder from an immutable,
 * accessible plugin.
 */
class PluginBuilder
{
private:
    std::string _name;
    std::vector<ComponentType> _types;

public:
    /**
     * @brief Construct a new Plugin Builder object
     *
     * Initializes the builder with an empty list of components and a name only.
     *
     * @param name The name of the plugin to construct.
     */
    explicit PluginBuilder(std::string name) : _name(std::move(name)) {}

    /**
     * @brief Adds a component factory
     *
     * @tparam T Component type, must be a subclass of @ref IComponent
     * @tparam Args Types of constructor arguments for @ref T
     * @param qualifiedName The qualified name of the component type
     * @param constructorArguments Constructor arguments for the component object
     * @return The updated builder object
     */
    template <typename T, typename... Args>
    PluginBuilder&
    addComponentType(const std::string& qualifiedName, Args&&... constructorArguments)
    {
        _types.push_back(ComponentType::create<T>(qualifiedName, constructorArguments...));
        return *this;
    }

    /**
     * @brief Builds a Plugin from the collected data
     *
     * @return The Plugin object that is defined by the name and the types
     */
    Plugin toPlugin() const { return Plugin(_name, _types); }
};

} // namespace mcf

#endif // MCF_PLUGIN_H