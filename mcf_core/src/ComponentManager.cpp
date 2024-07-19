/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/ComponentManager.h"
#include "mcf_core/ComponentTraceController.h"
#include "mcf_core/ErrorMacros.h"
#include "mcf_core/LoggingMacros.h"

namespace mcf {
void
PortProxy::connect()
{
    // enforce existence of the component
    auto c = _componentManager.getComponent(_componentId);
    _port.connect();
}

void
PortProxy::disconnect()
{
    auto c = _componentManager.getComponent(_componentId);
    _port.disconnect();
}

std::string
PortProxy::name() const
{
    auto c = _componentManager.getComponent(_componentId);
    return _port.getName();
}

std::string
PortProxy::topic() const
{
    auto c = _componentManager.getComponent(_componentId);
    return _port.getTopic();
}

void
PortProxy::mapToTopic(const std::string& topic)
{
    auto c = _componentManager.getComponent(_componentId);
    _componentManager.mapPort(c, name(), topic);
}

Port::Direction
PortProxy::direction() const
{
    auto c = _componentManager.getComponent(_componentId);
    return _port.getDirection();
}

bool
PortProxy::isConnected() const
{
    auto c = _componentManager.getComponent(_componentId);
    return _port.isConnected();
}

bool
PortProxy::isBlocking() const
{
    auto c          = _componentManager.getComponent(_componentId);
    auto queuedPort = dynamic_cast<GenericQueuedReceiverPort*>(&_port);
    if (queuedPort != nullptr)
    {
        return queuedPort->getBlocking();
    }
    return false;
}

void PortProxy::setBlocking(bool blocking)
{
    auto c          = _componentManager.getComponent(_componentId);
    auto queuedPort = dynamic_cast<GenericQueuedReceiverPort*>(&_port);
    if (queuedPort != nullptr)
    {
        queuedPort->setBlocking(blocking);
    }
    else
    {
        MCF_ERROR_NOFILELINE(
            "Cannot change blocking property on a non-queued port at topic {} from component {}",
            _port.getTopic(),
            _port.getComponent().getName());
    }
}

std::size_t PortProxy::maxQueueLength() const
{
    auto c          = _componentManager.getComponent(_componentId);
    auto queuedPort = dynamic_cast<GenericQueuedReceiverPort*>(&_port);
    if (queuedPort != nullptr)
    {
        return queuedPort->getMaxQueueLength();
    }
    MCF_ERROR_NOFILELINE(
        "Cannot query queue length from a non-queued port at topic {} from component {}",
        _port.getTopic(),
        _port.getComponent().getName());
    return 0;
}

void PortProxy::setMaxQueueLength(std::size_t length)
{
    auto c          = _componentManager.getComponent(_componentId);
    auto queuedPort = dynamic_cast<GenericQueuedReceiverPort*>(&_port);
    if (queuedPort != nullptr)
    {
        queuedPort->setMaxQueueLength(length);
    }
    else
    {
        MCF_ERROR_NOFILELINE(
            "Cannot change queue length on a non-queued port at topic {} from component {}",
            _port.getTopic(),
            _port.getComponent().getName());
    }
}

bool
PortProxy::isQueued() const
{
    auto c          = _componentManager.getComponent(_componentId);
    auto queuedPort = dynamic_cast<GenericQueuedReceiverPort*>(&_port);
    return queuedPort != nullptr;
}

std::vector<PortProxy>
ComponentProxy::ports()
{
    return _componentManager.getPorts(*this);
}

PortProxy
ComponentProxy::port(const std::string& name)
{
    return _componentManager.getPort(*this, name);
}

void
ComponentProxy::mapPort(const std::string& portName, const std::string& portTopic)
{
    _componentManager.mapPort(*this, portName, portTopic);
}

void
ComponentProxy::configure()
{
    _componentManager.configure(*this);
}

void
ComponentProxy::startup(bool connectPorts)
{
    _componentManager.startup(*this, connectPorts);
}

void
ComponentProxy::shutdown()
{
    _componentManager.shutdown(*this);
}

void
ComponentProxy::setSchedulingParameters(const IComponent::SchedulingParameters& parameters)
{
    _componentManager.setSchedulingParameters(*this, parameters);
}

ComponentManager::ComponentManager(
        ValueStore& valueStore,
        const std::string& configDir,
        std::shared_ptr<IidGenerator> idGenerator) :
    ComponentManager(valueStore, configDir, nullptr, std::move(idGenerator))
{
}

ComponentManager::ComponentManager(
        ValueStore& valueStore,
        std::vector<std::string> configDirs,
        ComponentTraceController* traceController,
        std::shared_ptr<IidGenerator> idGenerator) :
    fValueStore(valueStore),
    fComponentTraceController(traceController),
    fConfigDirs(std::move(configDirs)),
    fIdGenerator(std::move(idGenerator)),
    fNextComponentId(1)
{
    msg::ConfigDirs msgConfigDirs;
    msgConfigDirs.configDirs = fConfigDirs;
    fValueStore.setValue("/mcf/configdirectories", msgConfigDirs);

    // TODO: this is deprecated, because configs may be read from multiple directories now
    MCF_ASSERT(!fConfigDirs.empty(), "List of config directories must not be empty.");
    fValueStore.setValue("/mcf/configdirectory", msg::ConfigDir(fConfigDirs[0]));
}

ComponentManager::ComponentManager(
        ValueStore& valueStore,
        const std::string& configDir,
        ComponentTraceController* traceController,
        std::shared_ptr<IidGenerator> idGenerator) :
    fValueStore(valueStore),
    fComponentTraceController(traceController),
    fConfigDirs(1, configDir),
    fIdGenerator(std::move(idGenerator)),
    fNextComponentId(1)
{
    // make sure that the components' value store is different from the tracing value store
    if ((traceController != nullptr) && (&valueStore == &(traceController->getValueStore())))
    {
        MCF_ERROR("Value store used for tracing must be different from components' value store");
        throw std::runtime_error("Value store used for tracing must be different from components' value store");
    }

    msg::ConfigDirs msgConfigDirs;
    msgConfigDirs.configDirs = fConfigDirs;
    fValueStore.setValue("/mcf/configdirectories", msgConfigDirs);

    // TODO: this is deprecated, because configs may be read from multiple directories now
    fValueStore.setValue("/mcf/configdirectory", msg::ConfigDir(fConfigDirs[0]));
}

ComponentManager::~ComponentManager()
{
    shutdown();
}

ComponentProxy ComponentManager::registerComponent(std::shared_ptr<IComponent> component, const std::string* configName)
{
    return registerComponent(component, component->getName(), component->getName(), configName);
}

ComponentProxy
ComponentManager::registerComponent(std::shared_ptr<IComponent> component, const std::string& typeName, const std::string& instanceName, const std::string* configName)
{
    /*
    * Notes on thread safety:
    * The internal state that is modified here is
    * - ComponentManager::fComponents (std::map, not thread safe)
    * - ComponentManager::fNextComponentId (std::atomic, thread safe)
    * -> locking required, but only for container modification
    */
    if (!component)
    {
        MCF_WARN_NOFILELINE("An empty component was supplied, refusing to register it.");
        throw std::runtime_error("Invalid pointer to component");
    }
    component->ctrlSetConfigDirs(fConfigDirs);
    component->setIdGenerator(fIdGenerator);

    // create and set component trace event generator
    auto eventGenerator = std::unique_ptr<ComponentTraceEventGenerator>(nullptr);
    if (fComponentTraceController != nullptr)
    {
        eventGenerator = fComponentTraceController->createEventGenerator(instanceName);
    }
    component->ctrlSetComponentTraceEventGenerator(std::move(eventGenerator));

    // set config name if given, otherwise use default of component
    if (configName != nullptr)
    {
        component->ctrlSetConfigName(*configName);
    }
    auto componentId = fNextComponentId.fetch_add(1);
    auto descriptor  = ComponentProxy(instanceName, typeName, componentId, *this);
    {
        std::lock_guard<std::recursive_mutex> lk(fMutex);
        fComponents.insert(std::make_pair(
            componentId, ComponentMapEntry{descriptor, component, ComponentState::REGISTERED}));
    }
    return descriptor;
}

void ComponentManager::registerPort(Port& port)
{
    // Thread safety: Internal state is modified for fComponents, so the iterator must be unchanged
    // -> method body should be guarded
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    // find component id
    auto it = std::find_if(fComponents.begin(), fComponents.end(), [&port](const auto& cme) {
        return cme.second.component.get() == &port.getComponent();
    });
    if (fComponentPortMap[it->first].find(port.getName()) != fComponentPortMap[it->first].end())
    {
        MCF_WARN_NOFILELINE("A port with name {} is already present", port.getName());
        throw std::runtime_error("Attempt to register a port with the same name");
    }
    fComponentPortMap[it->first].insert(std::make_pair(port.getName(), PortMapEntry{port, false}));
}

void ComponentManager::registerPort(Port& port, const std::string& topic)
{
    // Thread safety: registerPort() is thread safe, port mapping is handled at a different place
    registerPort(port);
    port.mapToTopic(topic);
}

bool ComponentManager::configure()
{
    // Thread safety: Locking the method ensures that the configuration has not changed between
    // calls; the private methods can then assume that the lock exists.
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    callConfigure();
    setupPorts();
    return checkConfiguration();
}

void ComponentManager::configure(const ComponentProxy& descriptor)
{
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    Config config(*this, descriptor.name());
    auto& entry = fComponents.at(descriptor.id());
    if (entry.state == ComponentState::REGISTERED)
    {
        entry.component->ctrlConfigure(config);
        // setup ports in configure() rather than in startup()
        for (auto& me : fComponentPortMap[descriptor.id()])
        {
            me.second.port.setup(fValueStore);
        }
        entry.state = ComponentState::CONFIGURED;
    }
    else 
    {
        // the component is already configured
        MCF_WARN_NOFILELINE(
            "The component {}:{} is already configured, refusing to configure it again",
            descriptor.name(),
            descriptor.typeName());
    }
}

void ComponentManager::startup(bool connectPorts)
{
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    if (connectPorts)
    {
        this->connectPorts();
    }
    auto componentsToStart = std::vector<uint64_t>();
    componentsToStart.reserve(fComponents.size());
    for (auto& c : fComponents)
    {
        if (c.second.state == ComponentState::CONFIGURED)
        {
            componentsToStart.push_back(c.first);
            c.second.component->ctrlStart();
        }
    }
    bool doWait = true;
    while(doWait)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        doWait = false;
        for (const auto& id: componentsToStart)
        {
            if (fComponents.at(id).component->getState() != IComponent::STARTED)
            {
                doWait = true;
            }
        }
    }
    for (const auto& id : componentsToStart)
    {
        fComponents.at(id).component->ctrlRun();
        fComponents.at(id).state = ComponentState::RUNNING;
    }
}

void ComponentManager::startup(const ComponentProxy& descriptor, bool connectPorts)
{
    /*
     * validate the component's ports setup
     * Since checkConfiguration() only enables, but never disables ports, the only outcomes here can be
     * - a port of any new, non-started component is validated (isValid == true in the port map)
     * - a port of any new, non-started component is not validated
     * Already validated ports are not changed wrt isValid flag.
     */
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    auto& entry = fComponents.at(descriptor.id());
    if (entry.state == ComponentState::CONFIGURED)
    {
        checkConfiguration();
        // setup and connect ports
        auto component = fComponents.at(descriptor.id()).component;
        for (auto me : fComponentPortMap[descriptor.id()])
        {
            if (me.second.isValid)
            {
                me.second.port.connect();
            }
        }
        component->ctrlStart();
        bool doWait = true;
        while (doWait) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (component->getState() == IComponent::STARTED) {
                doWait = false;
            }
        }
        component->ctrlRun();
        entry.state = ComponentState::RUNNING;
    }
    else
    {
        if (entry.state == ComponentState::REGISTERED)
        {
            MCF_THROW_RUNTIME(fmt::format(
                "Component {}:{} is not configured and cannot be started",
                descriptor.name(),
                descriptor.typeName()));
        }
    }
}

void ComponentManager::shutdown()
{
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    for (auto& c : fComponents) {
        if (c.second.state == ComponentState::RUNNING)
        {
            MCF_INFO_NOFILELINE("Component Manager: stopping component {}", c.second.component->getName());
            for (auto& p: getPorts(c.second.descriptor))
            {
                p.disconnect();
            }
            c.second.component->ctrlStop();
            c.second.state = ComponentState::CONFIGURED;
        }
    }
}

void ComponentManager::shutdown(const ComponentProxy& descriptor)
{
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    auto& entry    = fComponents.at(descriptor.id());
    if (entry.state == ComponentState::RUNNING)
    {
        MCF_INFO_NOFILELINE("Component Manager: stopping component {}", entry.component->getName());
        auto ports     = getPorts(descriptor);
        for (auto& p: ports)
        {
            p.disconnect();
        }
        entry.component->ctrlStop();
        entry.state = ComponentState::CONFIGURED;
    }
}

void ComponentManager::eraseComponent(const ComponentProxy& descriptor)
{
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    auto component = fComponents.at(descriptor.id()).component;
    auto ports     = getPorts(descriptor);
    for (auto& p: ports)
    {
        p.disconnect();
    }
    if (component->getState() != IComponent::STOPPED)
    {
        component->ctrlStop();
    }
    // remove ports
    fComponents.erase(descriptor.id());
    fComponentPortMap.erase(descriptor.id());
}

void
ComponentManager::setSchedulingParameters(
    const ComponentProxy& proxy, const IComponent::SchedulingParameters& parameters)
{
    fComponents.at(proxy.id()).component->ctrlSetSchedulingParameters(parameters);
}

void
ComponentManager::setComponentLogLevels(
    const std::string& componentName,
    LogSeverity consoleLevel,
    LogSeverity valueStoreLevel)
{
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    for (const auto& c : fComponents)
    {
        if (c.second.descriptor.name() == componentName)
        {
            c.second.component->ctrlSetLogLevels(consoleLevel, valueStoreLevel);
        }
    }
}

void ComponentManager::setGlobalLogLevels(
    LogSeverity consoleLevel,
    LogSeverity valueStoreLevel)
{
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    for (const auto& c : fComponents)
    {
        c.second.component->ctrlSetLogLevels(consoleLevel, valueStoreLevel);
    }
}

std::vector<ComponentProxy> ComponentManager::getComponents() const
{
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    std::vector<ComponentProxy> result;

    std::transform(
        fComponents.begin(), fComponents.end(), std::back_inserter(result), [](const auto& c) {
            return c.second.descriptor;
        });
    return result;
}

ComponentProxy ComponentManager::getComponent(uint64_t instanceId) const
{
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    if (fComponents.find(instanceId) == fComponents.end())
    {
        MCF_ERROR_NOFILELINE("Component with ID {} requested but it does not exist", instanceId);
        throw std::runtime_error("Invalid component id");
    }
    
    return fComponents.at(instanceId).descriptor;
}

std::vector<PortProxy> ComponentManager::getPorts(const ComponentProxy& descriptor)
{
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    auto result = std::vector<PortProxy>();
    const ComponentMapEntry& entry = fComponents.at(descriptor.id());
    auto ports                     = fComponentPortMap.find(descriptor.id());
    if (ports != fComponentPortMap.end())
    {
        for (const auto& pair : ports->second)
        {
            result.emplace_back(pair.second.port, descriptor.id(), *this);
        }
    }
    return result;
}

ComponentTraceController* ComponentManager::getComponentTraceController()
{
    return fComponentTraceController;
}

PortProxy
ComponentManager::getPort(const ComponentProxy& descriptor, const std::string& name)
{
    const auto id = descriptor.id();
    return PortProxy(fComponentPortMap.at(id).at(name).port, id, *this);
}

void
ComponentManager::mapPort(
    const ComponentProxy& proxy, const std::string& portName, const std::string& topicName)
{
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    auto componentInstanceId = proxy.id();
    auto& ports              = fComponentPortMap.at(componentInstanceId);
    if (ports.find(portName) == ports.end())
    {
        const auto& componentName = fComponents.at(componentInstanceId).descriptor.name();
        MCF_ERROR_NOFILELINE("Port {} not registered with component {}", portName, componentName);
        throw std::runtime_error("Access to absent or non-registered port");
    }
    auto& entry = ports.at(portName);
    entry.port.mapToTopic(topicName);
    if (isTopicValid(topicName))
    {
        entry.isValid = true;
    }
}

bool
ComponentManager::validateConfiguration()
{
    std::lock_guard<std::recursive_mutex> lk(fMutex);
    return checkConfiguration();
}

bool ComponentManager::checkConfiguration()
{
    // private method, no locking required
    std::map<std::string, std::vector<PortMapEntry*>> portsByTopic;

    for (auto& idMapPair: fComponentPortMap)
    {
        for (auto& nameEntryPair: idMapPair.second)
        {
            auto& me = nameEntryPair.second;
            portsByTopic[me.port.getTopic()].push_back(&me);
        }
    }

    /*
     * What happens here is the following:
     * For every topic, we check
     * - if it is valid (required)
     * - if all connected ports have the same type (required)
     * - if there are both receivers and senders (will issue a warning if violated)
     * If the topic is empty, the ports are not enabled, but this will merely produce a warning, not
     * an error
     */
    bool result = true;
    for (auto& pair : portsByTopic) {
        const auto& topic = pair.first;
        auto pmes = pair.second;

        if (topic.empty()) {
            continue;
        }

        std::vector<Port*> ports;
        std::transform(pmes.begin(), pmes.end(), std::back_inserter(ports),
            [](PortMapEntry* pme) { return &pme->port; });

        bool perTopicResult = true;
//        if (!typesCompatible(ports)) {
//            MCF_ERROR_NOFILELINE("Incompatible types for topic {}", topic);
//            perTopicResult = false;
//        }

        if (!isTopicValid(topic)) {
            MCF_ERROR_NOFILELINE("Port(s) attached to invalid topic: `{}`", topic);
            for (const auto& p: pmes) {
                const auto it
                    = std::find_if(fComponents.begin(), fComponents.end(), [&p](const auto& pair) {
                          return pair.second.component.get() == &p->port.getComponent();
                      });
                MCF_ERROR_NOFILELINE("    {}:{}", it->second.descriptor.name(), p->port.getName());
            }
            perTopicResult = false;
        }

        int numSenders = std::count_if(ports.begin(), ports.end(),
            [](Port* port){return port->getDirection() == Port::sender ;});

        int numReceivers = std::count_if(ports.begin(), ports.end(),
            [](Port* port){return port->getDirection() == Port::receiver ;});

        if (numSenders == 0) {
            MCF_WARN_NOFILELINE("No sender for topic {}", topic);
        }
        else if (numSenders > 1) {
            MCF_WARN_NOFILELINE("More than one sender for topic {}", topic);
        }

        if (numReceivers == 0) {
            MCF_WARN_NOFILELINE("No receiver for topic {}", topic);
        }

        if (perTopicResult) {
            // validate all ports connected to this topic
            for (auto pme : pmes) {
                pme->isValid = true;
            }
        }
        else {
            // error in at least one topic
            result = false;
        }
    }

    return result;
}

bool ComponentManager::typesCompatible(const std::vector<Port*>& ports)
{
    // static function, no locking required
    bool initialized = false;
    std::type_index currentTypeIndex = std::type_index(typeid(0));
    for (const auto& port : ports) {
      std::type_index typeIndex = port->getTypeIndex();
        if (initialized) {
            if (typeIndex != currentTypeIndex) {
                MCF_WARN_NOFILELINE("Incompatible types: {} vs {}", typeIndex.name(), currentTypeIndex.name());
                return false;
            }
        }
        currentTypeIndex = typeIndex;
        initialized = true;
    }
    return true;
}

void ComponentManager::setupPorts()
{
    // private method, no locking required
    for (auto& idMapPair: fComponentPortMap)
    {
        for (auto& nameEntryPair: idMapPair.second)
        {
            auto& me = nameEntryPair.second;
            me.port.setup(fValueStore);
        }
    }
}

void ComponentManager::connectPorts()
{
    // private method, no locking required
    for (auto& idMapPair : fComponentPortMap)
    {
        for (auto& nameEntryPair : idMapPair.second)
        {
            auto& me = nameEntryPair.second;
            if (me.isValid)
            {
                me.port.connect();
            }
        }
    }
}

void ComponentManager::callConfigure()
{
    // private method, no locking required
    for (auto& c : fComponents) {
        if (c.second.state == ComponentState::REGISTERED)
        {
            Config config(*this, c.second.descriptor.name());
            c.second.component->ctrlConfigure(config);
            c.second.state = ComponentState::CONFIGURED;
        }
    }
}

bool ComponentManager::isTopicValid(const std::string& topicName)
{
    bool empty = topicName.empty();
    bool validCharacters = std::all_of(topicName.begin(), topicName.end(), [](const char& c) {
        return (std::isprint(c) != 0) && (std::isspace(c) == 0);
    });

    return !empty && validCharacters;
}

ComponentManager::Config::Config(ComponentManager& componentManager, std::string instanceName)
: fComponentManager(componentManager), fInstanceName(std::move(instanceName))
{}

void
ComponentManager::Config::registerPort(Port& port)
{
    port.setComponentTraceEventGenerator(
            port.getComponent().getComponentTraceEventGenerator());
    fComponentManager.registerPort(port);
}

void ComponentManager::Config::registerPort(Port& port, const std::string& key) {
    port.setComponentTraceEventGenerator(
            port.getComponent().getComponentTraceEventGenerator());
    fComponentManager.registerPort(port, key);
}

}  // namespace mcf