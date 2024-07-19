/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/SystemConfigurator.h"

#include "mcf_core/Mcf.h"
#include "json/json.h"

#include <fstream>

namespace mcf
{
system_configuration::ComponentSystem
ComponentSystemConfigurator::readSystemConfiguration(const Json::Value& node)
{
    system_configuration::ComponentSystem config;
    Json::Value componentConfigs = node["Components"];
    config.reserve(componentConfigs.size());

    for (Json::ValueIterator instanceNameTypeIterator = componentConfigs.begin();
         instanceNameTypeIterator != componentConfigs.end();
         ++instanceNameTypeIterator)
    {
        std::string instanceName = instanceNameTypeIterator.key().asString();
        std::string typeName = instanceNameTypeIterator->get("type", Json::Value("")).asString();

        std::vector<system_configuration::PortMapping> portMappings;

        auto value    = *instanceNameTypeIterator;
        auto mappings = value["portMapping"];
        portMappings.reserve(mappings.size());
        for (auto portNameTopicIterator = mappings.begin(); portNameTopicIterator != mappings.end();
             ++portNameTopicIterator)
        {
            auto portName    = portNameTopicIterator.key().asString();
            auto topicObject = *portNameTopicIterator;
            bool connected   = true;

            if (portNameTopicIterator->isObject())
            {
                topicObject = portNameTopicIterator->get("topic", Json::Value());
                connected   = portNameTopicIterator->get("connected", true).asBool();
            }

            auto topic = topicObject.asString();
            if (topic.empty())
            {
                connected = false;
            }
            portMappings.push_back(system_configuration::PortMapping{portName, topic, connected});
        }

        IComponent::SchedulingParameters schedulingParameters{
            IComponent::SchedulingPolicy::Default, 0};
        Json::Value parametersDeclaration = value.get("schedulingParameters", Json::Value());
        if (!parametersDeclaration.empty())
        {
            // convert policy and priority
            auto policy = parametersDeclaration.get("policy", Json::Value("other")).asString();
            schedulingParameters.priority
                = parametersDeclaration.get("priority", Json::Value(0)).asInt();

            if (policy == "other")
            {
                schedulingParameters.policy = IComponent::SchedulingPolicy::Other;
            }
            else if (policy == "fifo")
            {
                schedulingParameters.policy = IComponent::SchedulingPolicy::Fifo;
            }
            else if (policy == "round-robin")
            {
                schedulingParameters.policy = IComponent::SchedulingPolicy::RoundRobin;
            }
            else if (policy == "default")
            {
                schedulingParameters.policy = IComponent::SchedulingPolicy::Default;
            }
            else
            {
                throw SystemConfigurationError("Component scheduling policy must be one of "
                                               "'other', 'fifo', 'round-robin', 'default'");
            }
        }

        config.push_back(system_configuration::ComponentInstance{
            instanceName, typeName, schedulingParameters, portMappings});
    }

    return config;
}

system_configuration::ComponentSystem
ComponentSystemConfigurator::readSystemConfiguration(std::istream& stream)
{
    auto reader               = Json::CharReaderBuilder();
    reader["collectComments"] = false;

    Json::Value root;
    std::string errors;
    bool success = Json::parseFromStream(reader, stream, &root, &errors);

    if (success)
    {
        system_configuration::ComponentSystem config;
        Json::Value componentConfigs = root["ComponentSystemConfiguration"];
        return readSystemConfiguration(componentConfigs);
    }
    MCF_ERROR_NOFILELINE("System configuration JSON could not be parsed: {}", errors);
    throw SystemConfigurationError("Could not parse system configuration JSON");
}

void
ComponentSystemConfigurator::configure(const system_configuration::ComponentSystem& configuration)
{
    auto components                                    = _manager.getComponents();
    std::map<std::string, ComponentProxy> componentMap = std::map<std::string, ComponentProxy>();
    for (const auto& proxy : components)
    {
        componentMap.insert(std::make_pair(proxy.name(), proxy));
    }

    auto getComponent
        = [&componentMap, this](
              const std::string& type, const std::string& instance) -> ComponentProxy {
        auto it = componentMap.find(instance);
        if (it != componentMap.end())
        {
            auto proxy = it->second;
            if (type.empty() || proxy.typeName() == type)
            {
                // type name is either not set or consistent with the current configuration
                return proxy;
            }
            else
            {
                throw SystemConfigurationError(
                    "A component with same name but different type already exists");
            }
        }
        else
        {
            if (type.empty())
            {
                throw SystemConfigurationError("Cannot instantiate component with empty type");
            }
            if (instance.empty())
            {
                throw SystemConfigurationError(
                    "Cannot instantiate component with empty instance name");
            }
            auto proxy = _instantiator.createComponent(type, instance);
            proxy.configure();
            return proxy;
        }
    };

    std::vector<std::string> errors;
    auto instantiatedComponents = std::vector<ComponentProxy>();
    for (const auto& component : configuration)
    {
        try
        {
            // check if the component is present
            auto proxy = getComponent(component.typeName, component.instanceName);
            instantiatedComponents.push_back(proxy);
            proxy.setSchedulingParameters(component.schedulingParameters);

            // attach config in and out port to default topics
            const auto configTopic = Component::DEFAULT_CONFIG_TOPIC_PATH + component.instanceName;
            proxy.mapPort("ConfigIn", configTopic);
            proxy.mapPort("ConfigOut", configTopic);

            for (const auto& mapping : component.portMappings)
            {
                proxy.mapPort(mapping.portName, mapping.portTopic);
                if (mapping.connected)
                {
                    proxy.port(mapping.portName).connect();
                }
            }
        }
        catch (const SystemConfigurationError& error)
        {
            errors.push_back(fmt::format(
                "{} ({}): {}", component.instanceName, component.typeName, error.what()));
        }
        catch (const ComponentInstantiationError& error)
        {
            errors.push_back(fmt::format(
                "{} ({}): {}", component.instanceName, component.typeName, error.what()));
        }
    }
    for (const auto& message : errors)
    {
        MCF_ERROR_NOFILELINE(message);
    }
    // check for errors, if no errors are present, validate configuration
    if (!errors.empty() || !_manager.validateConfiguration())
    {
        MCF_ERROR_NOFILELINE(
            "System configuration invalid! Removing partially initialized components.");
        // de-instantiate already instantiated components
        for (const auto& component : instantiatedComponents)
        {
            auto name = component.name();
            try
            {
                _instantiator.removeComponent(name);
            }
            catch (const ComponentInstantiationError& error)
            {
                MCF_ERROR_NOFILELINE("Could not remove an instantiated component {}", name);
            }
        }

        std::stringstream out;
        for (auto it = errors.begin(); it != errors.end();)
        {
            out << *it;
            ++it;
            if (it != errors.end())
            {
                out << "\n";
            }
        }
        throw SystemConfigurationError(out.str());
    }
}

void
ComponentSystemConfigurator::configureFromFile(const std::string& fileName)
{
    auto istream = std::ifstream(fileName, std::ios::in);
    configure(readSystemConfiguration(istream));
}

void
ComponentSystemConfigurator::configureFromJSON(const std::string& jsonString)
{
    auto istream = std::istringstream(jsonString);
    configure(readSystemConfiguration(istream));
}

void
ComponentSystemConfigurator::configureFromJSONNode(const Json::Value& node)
{
    configure(readSystemConfiguration(node));
}

} // namespace mcf