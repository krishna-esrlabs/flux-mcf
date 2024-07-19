/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_COMPONENT_INSTANTIATOR_H
#define MCF_COMPONENT_INSTANTIATOR_H

#include "mcf_core/ComponentManager.h"
#include "mcf_core/ComponentType.h"

namespace mcf
{
/**
 * @brief An exception class for errors in component instantiation.
 *
 */
class ComponentInstantiationError : public std::runtime_error
{
public:
    ComponentInstantiationError(const std::string& what) : std::runtime_error(what){};
};

/**
 * @brief A class that manages the "outer" lifecycle of a component, namely its creation and
 * destruction.
 *
 */
class ComponentInstantiator
{
private:
    struct ComponentInstance
    {
        explicit ComponentInstance(
            ComponentProxy& _proxy, std::shared_ptr<IComponent>& _component, ComponentType& _type)
        : proxy(_proxy), component(_component), type(_type)
        {
        }

        ComponentProxy proxy;
        std::shared_ptr<IComponent> component;
        ComponentType type;
    };

    /// The component manager instance that cares about
    ComponentManager& _manager;
    /// Maps qualified names to types
    std::map<std::string, ComponentType> _types;
    /// A list of already created components, mapped to their respective types
    std::list<ComponentInstance> _instances;

public:
    explicit ComponentInstantiator(ComponentManager& manager);

    void addComponentType(const ComponentType& type);
    void removeComponentType(const std::string& qualifiedName);

    /**
     * @brief Creates and registers a Component with the component manager
     *
     * @param qualifiedName The qualified name of the component type
     * @param instanceName The instance name of the component type
     * @return A ComponentProxy object that refers to the component instance
     */
    ComponentProxy
    createComponent(const std::string& qualifiedName, const std::string& instanceName);

    /**
     * @brief Removes a component from the lifecycle, given its instance name
     *
     * @param instanceName The instance name of the component.
     */
    void removeComponent(const std::string& instanceName);

    std::vector<ComponentProxy> listComponents() const;

    /**
     * @brief Reloads an already instantiated component
     *
     * @param instanceName The instance name of the component
     * @return The ComponentProxy for the new instance
     */
    ComponentProxy reloadComponent(const std::string& instanceName);

    std::vector<std::string> listComponentTypes() const;
    std::vector<std::string> listComponentTypes(const std::string& nameSpace) const;
};
} // namespace mcf

#endif // MCF_COMPONENT_INSTANTIATOR_H