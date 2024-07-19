/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/ComponentInstantiator.h"

#include "mcf_core/Component.h"
#include "mcf_core/LoggingMacros.h"

namespace mcf
{
ComponentInstantiator::ComponentInstantiator(ComponentManager& manager) : _manager(manager) {}

void
ComponentInstantiator::addComponentType(const ComponentType& type)
{
    // check if such type already exists
    if (_types.find(type.qualifiedName()) != _types.end())
    {
        MCF_ERROR_NOFILELINE("Component type {} has been already registered", type.qualifiedName());
        throw ComponentInstantiationError("Type is already registered");
    }
    _types.insert(std::make_pair(type.qualifiedName(), type));
}

void
ComponentInstantiator::removeComponentType(const std::string& qualifiedName)
{
    // check if such type is registered
    if (_types.find(qualifiedName) == _types.end())
    {
        MCF_ERROR_NOFILELINE("Component type {} has not been registered", qualifiedName);
        throw ComponentInstantiationError("Type is already registered");
    }
    // remove all instances
    auto it = _instances.begin();
    while (it != _instances.end())
    {
        if (it->type.qualifiedName() == qualifiedName)
        {
            _manager.eraseComponent(it->proxy);
            it = _instances.erase(it);
        }
        else
        {
            ++it;
        }
    }
    _types.erase(qualifiedName);
}

ComponentProxy
ComponentInstantiator::createComponent(
    const std::string& qualifiedName, const std::string& instanceName)
{
    // check for uniqueness of instance name
    auto it
        = std::find_if(_instances.begin(), _instances.end(), [&instanceName](const auto& instance) {
              return instance.proxy.name() == instanceName;
          });
    if (it != _instances.end())
    {
        MCF_ERROR_NOFILELINE("Instance {} already present", instanceName);
        throw ComponentInstantiationError("Component instance already present");
    }
    if (_types.find(qualifiedName) == _types.end())
    {
        MCF_ERROR_NOFILELINE("Type {} not registered, cannot instantiate", qualifiedName);
        throw ComponentInstantiationError("Type cannot be instantiated");
    }
    auto type                            = _types.at(qualifiedName);
    std::shared_ptr<IComponent> instance = type.makeInstance();
    std::string configName = instanceName + Component::DEFAULT_CONFIG_NAME_SUFFIX;
    ComponentProxy proxy = _manager.registerComponent(instance,
                                                      qualifiedName,
                                                      instanceName,
                                                      &configName);

    _instances.emplace_back(proxy, instance, type);

    return proxy;
}

void
ComponentInstantiator::removeComponent(const std::string& instanceName)
{
    auto it
        = std::find_if(_instances.begin(), _instances.end(), [&instanceName](const auto& instance) {
              return instance.proxy.name() == instanceName;
          });
    if (it == _instances.end())
    {
        MCF_ERROR_NOFILELINE("Instance {} not found", instanceName);
        throw ComponentInstantiationError("Component instance not found");
    }
    else
    {
        _manager.eraseComponent(it->proxy);
        _instances.erase(it);
    }
}

ComponentProxy
ComponentInstantiator::reloadComponent(const std::string& instanceName)
{
    auto it
        = std::find_if(_instances.begin(), _instances.end(), [&instanceName](const auto& instance) {
              return instance.proxy.name() == instanceName;
          });
    if (it == _instances.end())
    {
        MCF_ERROR_NOFILELINE("Instance {} not found", instanceName);
        throw ComponentInstantiationError("Component instance not found");
    }
    else
    {
        auto qualifiedName = it->type.qualifiedName();
        removeComponent(instanceName);
        return createComponent(qualifiedName, instanceName);
    }
}

std::vector<ComponentProxy>
ComponentInstantiator::listComponents() const
{
    auto components = std::vector<ComponentProxy>();
    std::transform(
        _instances.begin(),
        _instances.end(),
        std::back_inserter(components),
        [](const auto& instance) { return instance.proxy; });
    return components;
}

std::vector<std::string>
ComponentInstantiator::listComponentTypes() const
{
    auto typeNames = std::vector<std::string>();
    std::transform(
        _types.begin(), _types.end(), std::back_inserter(typeNames), [](const auto& type) {
            return type.first;
        });
    return typeNames;
}

std::vector<std::string>
ComponentInstantiator::listComponentTypes(const std::string& nameSpace) const
{
    auto typeNames = std::vector<std::string>();
    for (const auto& type : _types)
    {
        if (type.second.nameSpace() == nameSpace)
        {
            typeNames.push_back(type.first);
        }
    }
    return typeNames;
}

} // namespace mcf