/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_COMPONENT_MANAGER_H
#define MCF_COMPONENT_MANAGER_H

#include <thread>
#include <chrono>
#include <string>

#include "mcf_core/IComponentConfig.h"
#include "mcf_core/Port.h"
#include "mcf_core/DefaultIdGenerator.h"

namespace mcf {
/**
 * @brief A proxy class for ports
 *
 * Objects of this class are produced by the ComponentManager and are valid on creation. Each method
 * is guaranteed to either return a valid result or to throw an exception as long as the
 * ComponentManager is alive. If one requested a PortProxy object and destroyed the ComponentManager
 * while retaining the proxy object, the program is in an (unrecoverable) error state.
 */
class PortProxy final
{
private:
    /// Port reference
    Port& _port;
    /// Internal component id
    uint64_t _componentId;
    /// Reference to the component manager
    ComponentManager& _componentManager;

public:
    /**
     * @brief Construct a new Port Proxy object
     *
     * As with ComponentProxy, this object is not meant to be directly constructed, but rather dealt
     * out by ComponentProxy or ComponentManager instances.
     *
     * @param port Reference to a registered port object
     * @param componentId Internal instance id of the component in the component manager
     * @param componentManager Reference to the component manager
     */
    PortProxy(Port& port, const uint64_t componentId, ComponentManager& componentManager)
    : _port(port), _componentId(componentId), _componentManager(componentManager)
    {
    }

    void connect();

    void disconnect();

    std::string name() const;

    std::string topic() const;

    void mapToTopic(const std::string& topic);

    Port::Direction direction() const;

    bool isConnected() const;

    bool isBlocking() const;

    void setBlocking(bool blocking);

    std::size_t maxQueueLength() const;

    void setMaxQueueLength(std::size_t length);

    bool isQueued() const;
};

/**
 * @brief A proxy/descriptor class for components.
 *
 * Instances of this class are produced upon information queries to ComponentManager. This means
 * that only ComponentManager should produce these descriptors, they are not meant to be created by
 * hand.
 * 
 */
class ComponentProxy final
{
public:
    /**
     * @brief Construct a new Component Proxy object
     * 
     * For regular usage, ComponentManager::getComponents() should be used.
     * 
     * Convenience call if no type name is known (then, the type name will be set to the instance name)
     * 
     * @param name The name of the component
     * @param id The internal id of the component
     * @param componentManager Reference to the component manager instance
     */
    ComponentProxy(
        const std::string& name,
        uint64_t id,
        ComponentManager& componentManager)
    : _name(name), _typeName(name), _id(id), _componentManager(componentManager)
    {
    }

    /**
     * @brief Construct a new Component Proxy object
     *
     * Called from ComponentManager, not meant to be called directly. Use
     * ComponentManager::getComponents() to retrieve current proxy objects.
     *
     * @param name The name of the component
     * @param typeName The type name of the component
     * @param id The internal id of the component
     * @param componentManager Reference to the component manager instance
     */
    ComponentProxy(
        const std::string name,
        const std::string typeName,
        uint64_t id,
        ComponentManager& componentManager)
    : _name(std::move(name))
    , _typeName(std::move(typeName))
    , _id(id)
    , _componentManager(componentManager)
    {
    }

    ComponentProxy() = delete;

    std::string name() const { return _name; }
    std::string typeName() const { return _typeName; }
    uint64_t id() const { return _id; }
    std::vector<PortProxy> ports();

    /**
     * @brief Retrieves a PortProxy for a given port name
     * 
     * Throws a runtime error if the port name does not exist
     * 
     * @param name The name of the port that is the part of the component
     * @return PortProxy The corresponding port proxy
     */
    PortProxy port(const std::string& name);

    /**
     * @brief Maps a port to a given topic
     * 
     * This method is only meaningful for a configured component object.
     * @param portName Name of the port
     * @param portTopic Topic to attach to
     */
    void mapPort(const std::string& portName, const std::string& portTopic);

    /**
     * @brief Configures the underlying component object
     *
     * After configure(), the ports are exposed to ComponentManager and attached to the value store
     * but not validated, which leads to them not being connected. The easiest way to do this
     * depends on the use case.
     * - If one needs to initialize many components, then after configuration,
     *   ComponentManager::validateConfiguration() shall be called
     * - If one needs to default-configure a set of components, then ComponentManager::configure()
     *   should be called. Note, however, that calling configure() twice is not supported yet.
     * - If a single component has to be started, then call after configuration
     *   ComponentProxy::startup()
     */
    void configure();

    /**
     * @brief Starts the underlying component object, validating the ports first.
     *
     * This method validates, if so desired, the ports of the system first, and does not connect
     * them if there is a type mismatch. Nonetheless, it is possible to manually connect ports with
     * PortProxy::connect().
     *
     * @param connectPorts if the startup should validate and connect the ports of the component.
     */
    void startup(bool connectPorts = true);

    /**
     * @brief Shuts the component object down, removing it from the message processing.
     *
     * After shutdown() is called, the component does not participate in the event system anymore.
     * Calls the method IComponent::shutdown() internally.
     */
    void shutdown();

    void setSchedulingParameters(const IComponent::SchedulingParameters& parameters);

private:
    std::string _name;
    std::string _typeName;
    uint64_t _id;
    ComponentManager& _componentManager;
};

class ComponentTraceController;
class ValueStore;

class ComponentManager {

public:

    #ifdef _WIN32
        static constexpr const char* DEFAULT_CONFIG_DIR = "configuration\\";
    #else
        static constexpr const char* DEFAULT_CONFIG_DIR = "configuration/";
    #endif

    ComponentManager(
            ValueStore& valueStore,
            const std::string& configDir,
            std::shared_ptr<IidGenerator> idGenerator);

    ComponentManager(
            ValueStore& valueStore,
            std::vector<std::string> configDirs,
            ComponentTraceController* traceController = nullptr,
            std::shared_ptr<IidGenerator> idGenerator  = std::make_shared<DefaultIdGenerator>());

    explicit ComponentManager(ValueStore& valueStore,
                              const std::string& configDir = DEFAULT_CONFIG_DIR,
                              ComponentTraceController* traceController = nullptr,
                              std::shared_ptr<IidGenerator> idGenerator = std::make_shared<DefaultIdGenerator>());

    ~ComponentManager();

    /*
     * Register a component with the manager.
     * Components must be registered in order to be part of the system.
     *
     * The method accepts a shared pointer to the component. This implies that the ComponentManager
     * object will assume that the component object will stay the same, i.e., even if someone
     * reassigns/resets the shared pointer object elsewhere, the ComponentManager will not be aware
     * of it.
     *
     * The method will refuse to register the object if the shared_ptr is invalid (i.e., points to
     * nullptr).
     */
    ComponentProxy registerComponent(std::shared_ptr<IComponent> component, const std::string* configName = nullptr);

    /**
     * @brief Register a component with the manager.
     *
     * A more general method that allows one to specify a component instance name, in order to be
     * able to spawn several components of the same type but with different names.
     *
     * @param component
     * @param instanceName
     * @param typeName
     * @param configName
     * @return ComponentProxy
     */
    ComponentProxy registerComponent(
        std::shared_ptr<IComponent> component,
        const std::string& typeName,
        const std::string& instanceName,
        const std::string* configName = nullptr);

    /*
     * Register a port with the component manager.
     * To be called by components within their configure() callback.
     */
    void registerPort(Port& port);

    /**
     * @brief Compatibility interface for port registration
     *
     * Basically analogous to the original interface, but here ports can be assigned to topics
     *
     * @param port The port
     * @param topic The name of the port topic, can be anything. It is the component/system
     * developer's responsibility to ensure correct port wiring.
     * 
     * @note This is a deprecated interface. 
     */
    void registerPort(Port& port, const std::string& topic);

    /*
     * Get component trace controller or nullptr.
     */
    ComponentTraceController* getComponentTraceController();

    /*
     * We don't want to give the component manager to the components
     * so we use a proxy object in the configure call instead
     */
    class Config : public IComponentConfig {
    public:
        explicit Config(ComponentManager& componentManager, std::string instanceName) ;
        void registerPort(Port& port) override;
        void registerPort(Port& port, const std::string& topic) override;
        std::string instanceName() const { return fInstanceName; }

    private:
        ComponentManager& fComponentManager;
        std::string fInstanceName;
    };

    /*
     * Configure components.
     *
     * This calls the component's configure methods and checks
     * the resulting overall configuration.
     *
     * The postcondition of this method is that no component is in an unconfigured state, i.e., all
     * components will have their IComponent::ctrlConfigure() methods called.
     *
     * The method sets the ports up to use the value store, but does not connect them
     *
     * Returns false if configuration is invalid.
     */
    bool configure();

    /**
     * @brief Configure a specific component
     *
     * Calls the configure() method and sets its ports up to use the value store (but does not
     * connect them). If the component has been considered to be already configured, nothing will
     * happen; the post-condition is that the component is configured.
     *
     * @param descriptor
     */
    void configure(const ComponentProxy& descriptor);

    /*
     * Starts components
     *
     * @param connectPorts if all valid ports should be connected. True by default, but can be set
     * to false if there are specific preferences about which ports to start.
     * 
     * Components that are not already started but are configured will be started.
     */
    void startup(bool connectPorts = true);

    /*
     * Starts an individual component
     * 
     * @param connectPorts if all valid ports should be connected. True by default, but can be set
     * to false if there are specific preferences about which ports to start.
     * 
     * If the component cannot be started, an exception will be thrown.
     */
    void startup(const ComponentProxy& descriptor, bool connectPorts = true);

    /*
     * Shuts down components
     *
     * This method stops all currently running components such that no component participates in
     * message exchange afterwards.
     */
    void shutdown();

    /*
     * @brief Shuts down an individual component
     * 
     * If the component was not started, nothing happens.
     */
    void shutdown(const ComponentProxy& descriptor);

    /*
     * Erases a component from the registry
     */
    void eraseComponent(const ComponentProxy& descriptor);

    /**
     * @brief Sets a component's scheduling options
     *
     * Changes the scheduling parameters of the component's thread during the component's runtime.
     * This call will have non-trivial effects only if the process has the permission to switch
     * scheduling policies.
     *
     * @param proxy A component descriptor object
     * @param parameters Scheduling parameters: scheduling policy and priority.
     */
    void setSchedulingParameters(
        const ComponentProxy& proxy, const IComponent::SchedulingParameters& parameters);

    /*
     * Sets the logging level for a component, if available
     */
    void setComponentLogLevels(
        const std::string& componentName,
        LogSeverity consoleLevel,
        LogSeverity valueStoreLevel);

    /*
     * Sets the global logging level for all currently available components
     */
    void setGlobalLogLevels(
        LogSeverity consoleLevel, LogSeverity valueStoreLevel);

    std::vector<ComponentProxy> getComponents() const;

    /**
     * @brief Get a component instance proxy
     * 
     * Throws a runtime error if the id is invalid
     * 
     * @param instanceId The identification number for the registered component instance
     * @return Corresponding proxy object
     */
    ComponentProxy getComponent(uint64_t instanceId) const;

    /**
     * @brief Get port proxies for a component
     *
     * Will throw an out of range exception if the proxy has an invalid ID (which might only happen
     * if you hold a proxy to an already expired and de-instantiated component)
     *
     * Note that the ports are known to the ComponentManager only after the component instance is
     * configured, so the ports may exist even the call returns an empty std::vector
     *
     * @param descriptor Component proxy to query
     * @return List of port proxy objects that are registered with the component
     */
    std::vector<PortProxy> getPorts(const ComponentProxy& descriptor);

    /**
     * @brief Get a specific port proxy for a component, queried by name
     * 
     * If either the proxy object or the port name are invalid, the call will result in an out of
     * range access exception
     * 
     * @param descriptor Component to query for the port
     * @param name Port name
     * @return Corresponding port proxy object
     */
    PortProxy getPort(const ComponentProxy& descriptor, const std::string& name);

    void
    mapPort(const ComponentProxy& proxy, const std::string& portName, const std::string& topicName);

    bool validateConfiguration();

private:
    /**
     * @brief The state of the component, as seen by the ComponentManager
     * 
     * There are three relevant states that a Component might have, from this perspective: 
     * - registered with the component manager, but not configured yet
     * - configured, but not running
     * - running
     * The initial state is REGISTERED.
     * 
     * These states have the following transitions:
     * 
     * from \ to   | REGISTERED | CONFIGURED  | RUNNING
     * ------------+------------+-------------+-----------
     * REGISTRED   |            | configure() | 
     * ------------+------------+-------------+-----------
     * CONFIGURED  |            |             | startup()
     * ------------+------------+-------------+-----------
     * RUNNING     |            | shutdown()  |
     * 
     * 
     */
    enum ComponentState
    {
        REGISTERED, //< The component has been registered with the component manager
        CONFIGURED, //< The component has been configured
        RUNNING, //< The component participates in the event loop
    };

    bool checkConfiguration();

    static bool typesCompatible(const std::vector<Port*>& ports);

    void setupPorts();

    void connectPorts();

    void callConfigure();

    static bool isTopicValid(const std::string& topicName);

    struct PortMapEntry {
        Port& port;
        bool isValid;
    };

    struct ComponentMapEntry {
        ComponentProxy descriptor;
        std::shared_ptr<IComponent> component;
        ComponentState state;
    };

    ValueStore& fValueStore;
    ComponentTraceController* fComponentTraceController;
    std::map<uint64_t, ComponentMapEntry> fComponents;
    std::map<uint64_t, std::map<std::string, PortMapEntry>> fComponentPortMap;

    std::vector<std::string> fConfigDirs;

    std::shared_ptr<IidGenerator> fIdGenerator;
    std::atomic<uint64_t> fNextComponentId;

    /**
     * @brief A mutex to lock the ComponentManager
     *
     * Some operations in the ComponentManager are callable from different threads, such as new
     * component addition/configuration. As they need to change internal state, thread safety must
     * be ensured.
     * 
     * This is a recursive mutex because of the call path 
     * ComponentManager::configure() (public, requires unchanged component list)
     * ComponentManager::callconfigure() (private)
     * Component::ctrlConfigure() (not part of this class)
     * Config::registerPort() (not part of this class)
     * CompoenentManager::registerPort() (public, requires unchanged component list)
     */
    mutable std::recursive_mutex fMutex;
};

} // namespace mcf

#endif // MCF_COMPONENT_MANAGER_H

