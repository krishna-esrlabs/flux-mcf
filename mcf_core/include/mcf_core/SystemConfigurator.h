/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_SYSTEM_CONFIGURATOR_H
#define MCF_SYSTEM_CONFIGURATOR_H

#include "mcf_core/ComponentInstantiator.h"
#include "mcf_core/ComponentManager.h"

#include "json/forwards.h"

#include <string>
#include <vector>

namespace mcf
{
/*
{
    "SystemConfiguration": {
        "Components": {
            "slamMot" : {
                "type": "SlamMot",
                "schedulingParameters": {
                    "policy": "fifo",
                    "priority": 7
                },
                "portMapping": {
                    "GPS": "/vehicle/GPS",
                    "Odometry": "/vehicle/odometry",
                    "Localization": "/localization/position",
                    "DebugImage": null,
                    "DebugData": {
                        "topic": "/debug/slammot",
                        "connected": false
                    }
                }
            }
        }
    }
}
*/

namespace system_configuration
{
struct PortMapping
{
    std::string portName;
    std::string portTopic;
    bool connected = true;
};

struct ComponentInstance
{
    std::string instanceName;
    std::string typeName;
    IComponent::SchedulingParameters schedulingParameters;
    std::vector<PortMapping> portMappings;
};

using ComponentSystem = std::vector<ComponentInstance>;
} // namespace system_configuration

class SystemConfigurationError : public std::runtime_error
{
public:
    explicit SystemConfigurationError(const std::string& what) : std::runtime_error(what) {}
};

/**
 * @brief Component system configuration mechanism
 *
 * Reads a declarative configuration description and either produces a configuration object or
 * configures the underlying system according to the description.
 */
class ComponentSystemConfigurator
{
private:
    ComponentManager& _manager;
    ComponentInstantiator& _instantiator;

public:
    /**
     * @brief Constructor
     *
     * @param manager The component manager instance
     * @param instantiator The component instantiation mechanism
     */
    ComponentSystemConfigurator(ComponentManager& manager, ComponentInstantiator& instantiator)
    : _manager(manager), _instantiator(instantiator)
    {
    }

    /**
     * @brief Reads the system configuration from a input stream object
     *
     * Parses the JSON in the std::istream, raising an exception on a parse error. The JSON must be
     * a superset of the example above.
     *
     * @param istream The std::istream to read the JSON configuration from
     * @return A system_configuration::ComponentSystem object describing the component system
     * configuration
     */
    system_configuration::ComponentSystem readSystemConfiguration(std::istream& istream);

    /**
     * @brief Reads the system configuration from a JSON (sub-)node.
     *
     * The sub-node must be of the shape
     * "Components": {
     *     "slamMot" : {
     *         "type": "SlamMot",
     *         "portMapping": {
     *             "GPS": "/vehicle/GPS",
     *             "Odometry": "/vehicle/odometry",
     *             "Localization": "/localization/position"
     *         }
     *     }
     * }
     *
     * @param node JSON object of the component configuration
     * @return system_configuration::ComponentSystem
     */
    system_configuration::ComponentSystem readSystemConfiguration(const Json::Value& node);

    /**
     * @brief Configures the controlled system according to the description object
     *
     * The precise semantics of configuration is as follows:
     * - Any component of type T and name N in the configuration that is not present in the current
     * system is instantiated and registered with the ComponentManager
     * - Any component of type T and name N in the configuration that is already present in the
     * current system is left untouched
     * - Any component of type T and name N for which a component of name N and type U already
     * exists will not be instantiated, resulting in an error
     * In case of an error, components that have been instantiated by the mechanism will be
     * de-instantiated; furthermore an error list will be printed to the log output.
     *
     * @param config The component system configuration description object
     */
    void configure(const system_configuration::ComponentSystem& config);

    /**
     * @brief Configures the component system from a file.
     *
     * Uses internally ComponentSystemConfigurator::readSystemConfiguration(std::istream&)
     *
     * @param fileName Name of the JSON file
     */
    void configureFromFile(const std::string& fileName);

    /**
     * @brief Configures the component system from a JSON string
     *
     * Uses internally ComponentSystemConfigurator::readSystemConfiguration(std::istream&)
     *
     * @param jsonString JSON which encodes the system configuration, of the format
     * "SystemConfiguration": {...}
     */
    void configureFromJSON(const std::string& jsonString);

    /**
     * @brief Configures a system from a JSON node
     *
     * Uses internally ComponentSystemConfigurator::readSystemConfiguration(const Json::Value&)
     *
     * @param node JSON object with "Components": {...} structure
     */
    void configureFromJSONNode(const Json::Value& node);
};

} // namespace mcf

#endif // MCF_SYSTEM_CONFIGURATOR_H