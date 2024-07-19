/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_COMPONENT_TYPE_H
#define MCF_COMPONENT_TYPE_H

#include "mcf_core/IComponent.h"

namespace mcf
{
/// An alias for a component constructor with bound parameters
using ComponentFactory = std::function<std::unique_ptr<IComponent>()>;

/**
 * @brief A type that reflects run-time component type information.
 *
 * The motivation for the existence for this type is our wish to be able to perform run-time
 * reflection at component level. If every component has its component type registered, then it is
 * possible to create new components at runtime or destroy all instances of a component.
 *
 * A component type is described at least by its qualified name of the form `namespace/name`
 */
class ComponentType
{
public:
    /**
     * @brief Construct a new Component Type object
     *
     * @param name The name of the component type
     * @param nameSpace The namespace (eg vendor)
     * @param factory The bound constructor function
     */
    ComponentType(std::string name, std::string nameSpace, ComponentFactory factory)
    : _name(std::move(name)), _nameSpace(std::move(nameSpace)), _factory(std::move(factory))
    {
    }

    /**
     * @brief Construct a new Component Type object
     *
     * Convenience constructor that is able to parse the qualified name.
     *
     * @param qualifiedName Component
     * @param factory
     */
    ComponentType(const std::string& qualifiedName, ComponentFactory factory)
    : _factory(std::move(factory))
    {
        setName(qualifiedName);
    }

    /**
     * @brief Convenience constructor template to bind constructor arguments
     *
     * Automatically parses the qualified name and binds the constructor arguments for the component
     * type
     * @tparam T The concrete component type
     * @tparam Args Component constructor argument types
     * @param qualifiedName Component qualified name (format: namespace/type)
     * @param constructorArguments Bound component constructor arguments
     */
    template <typename T, typename... Args>
    static ComponentType create(const std::string& qualifiedName, Args... constructorArguments)
    {
        auto factory
            = [constructorArguments...]() { return std::make_unique<T>(constructorArguments...); };
        return ComponentType(qualifiedName, factory);
    }

    std::string nameSpace() const { return _nameSpace; }

    /**
     * @brief Returns the qualified component type name
     *
     * @return Slash-separated qualified name, eg, esrlabs/TestComponent. If the namespace is empty,
     * returns just the type name.
     */
    std::string qualifiedName() const
    {
        if (_nameSpace.empty())
        {
            return _name;
        }
        else
        {
            return fmt::format("{}/{}", _nameSpace, _name);
        }
    }

    std::unique_ptr<IComponent> makeInstance() const { return _factory(); }

private:
    void setName(const std::string& qualifiedName)
    {
        std::size_t index = qualifiedName.find_last_of('/');
        if (index == std::string::npos)
        {
            _name      = qualifiedName;
            _nameSpace = "";
        }
        else
        {
            _nameSpace = qualifiedName.substr(0, index);
            _name      = qualifiedName.substr(index + 1);
        }
    }

    std::string _name;
    std::string _nameSpace;
    ComponentFactory _factory;
};
} // namespace mcf

#endif // MCF_COMPONENT_TYPE_H