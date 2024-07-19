/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_COMPONENT_H
#define MCF_COMPONENT_H

#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <memory>

#include "mcf_core/ValueStore.h"
#include "mcf_core/Messages.h"
#include "mcf_core/IComponent.h"
#include "mcf_core/Port.h"
#include "mcf_core/ComponentManager.h"
#include "mcf_core/ComponentLogger.h"
#include "mcf_core/LoggingMacros.h"
#include "mcf_core/ValueFactory.h"
#include "mcf_core/IdGeneratorInterface.h"

#include "json/forwards.h"

namespace mcf {

class ComponentTraceEventGenerator;

/** Base class for components.
 *
 * Component implementations should be derived from this class.
 *
 * There are two default entry points into component code:
 * - startup() is called on component startup
 * - shutdown() is called on component shutdown
 *
 * Further handlers can be registered.
 * All activations of these functions will be done in the same thread.
 *
 **/
class Component : public IComponent {

public:

    static constexpr const char* DEFAULT_CONFIG_NAME_SUFFIX = ".json";
    static constexpr const char* DEFAULT_CONFIG_TOPIC_PATH = "mcf/configs/";

    Component(const std::string& name, int priority = 1);

    ~Component() override;

    virtual void ctrlSetConfigName(const std::string& cfgName) {
      fConfigName = cfgName;
    };

    virtual void ctrlSetConfigDirs(const std::vector<std::string>& cfgDirs) {
        fConfigDirs = cfgDirs;
    };

    [[deprecated ("Explicit topic reference inside of a component is deprecated, use the system configuration mechanism")]]
    virtual void ctrlSetConfigOutPortTopic(const std::string& cfgTopic) {
        fConfigOutPortTopic = cfgTopic;
    };

    [[deprecated ("Explicit topic reference inside of a component is deprecated, use the system configuration mechanism")]]
    virtual void ctrlSetConfigInPortTopic(const std::string& cfgTopic) {
        fConfigInPortTopic = cfgTopic;
    };

    void ctrlConfigure(IComponentConfig& config) override;

    void ctrlStart() override;

    void ctrlRun() override;

    void ctrlSetComponentTraceEventGenerator(
            const std::shared_ptr<ComponentTraceEventGenerator>& eventGenerator) override;

    void ctrlStop() override;

    /**
     * @brief Sets the scheduling parameters of the component thread, checking them for plausibility
     *
     * This method throws an exception in the case if the parameters fail a runtime plausibility
     * check (min_priority <= priority <= max_priority for real-time schedulers, priority == 0 for
     * default scheduler).
     *
     * @sa IComponent::ctrlSetSchedulingParameters(const SchedulingParameters& parameters)
     *
     * @param parameters The scheduling parameters
     */
    void ctrlSetSchedulingParameters(const SchedulingParameters& parameters) override;

    std::string getName() const {
        return fName;
    }

    std::string getConfigName() const {
        return fConfigName;
    }

    msg::RuntimeStats getStatistics() const {
        return fRuntimeStats;
    }

    IComponent::StateType getState() const {
        return fState;
    }

    virtual void ctrlSetLogLevels(
        LogSeverity consoleLevel, LogSeverity valueStoreLevel)
    {
        fComponentLogger.setConsoleLogLevel(consoleLevel);
        fComponentLogger.setValueStoreLogLevel(valueStoreLevel);
    }

    virtual void injectLocalLogger()
    {
        fComponentLogger.injectLocalLogger();
    }

    virtual LogSeverity getConsoleLogLevel() const
    {
        return fComponentLogger.getConsoleLogLevel();
    }

    virtual LogSeverity getValueStoreLogLevel() const
    {
        return fComponentLogger.getValueStoreLogLevel();
    }

    void setIdGenerator(std::shared_ptr<IidGenerator> idGenerator)
    {
        fIdGenerator = idGenerator;
    }

    virtual const IidGenerator& idGenerator() const
    {
        return *fIdGenerator;
    }

    virtual const ValueFactory& valueFactory() const
    {
        return fValueFactory;
    }

    template<typename T>
    std::shared_ptr<const T> createValue(T& value)
    {
        fIdGenerator->injectId(value);
        return fValueFactory.createValue(value);
    }

    template<typename T>
    std::shared_ptr<const T> createValue(T&& value)
    {
        fIdGenerator->injectId(value);
        return fValueFactory.createValue(std::move(value));
    }

    template<typename T>
    std::shared_ptr<const T> createValue(std::unique_ptr<T>& value)
    {
        fIdGenerator->injectId(*value);
        return fValueFactory.createValue(value);
    }

    std::shared_ptr<ComponentTraceEventGenerator> getComponentTraceEventGenerator() const override
    {
        return fComponentTraceEventGenerator;
    }

protected:

    virtual void configure(IComponentConfig& config) {};
    virtual void startup() {};
    virtual void shutdown() {};

    /**
     * @return return first entry from list of config directories (deprecated)
     */
    std::string getConfigDir() const override;

    /**
     * @return list of config directories
     */
    std::vector<std::string> getConfigDirs() const;

    /**
     * Return the component configuration data from the component config file.
     * If the config file has not yet been read, it will be read here.
     * The method will fail with an exception, if reading fails
     *
     * The obtained configuration will be published automatically on the config output port
     */
    virtual const Json::Value& getConfig();

    /**
     * Get component configuration data from config input port
     *
     * The obtained configuration will *not* be published automatically on the config output port.
     *
     * @param config    the obtained configuration (if available)
     *
     * @return true, if configuration available, false otherwise.
      */
    virtual bool getConfigFromPort(Json::Value& config);

    /**
     * Read the config file and update the configuration data accordingly
     */
    virtual void readConfig();

    /*
     * Register a handler which runs everytime the component thread is triggered.
     */
    void registerTriggerHandler(std::function<void()> handler);

    /*
     * Trigger the components thread.
     */
    void trigger();

    void registerHandler(std::shared_ptr<PortTriggerHandler> handler) override;

    void unregisterHandler(std::shared_ptr<PortTriggerHandler> handler) override;

    ReceiverPort<msg::String>& getConfigInPort() {
        return fConfigInPort;
    }

    /*
     * Logging API
     */
    void log(mcf::LogSeverity severity, const std::string& message);

private:

    void main();

    typedef struct {
        std::function<void(void)> handler;
        msg::RuntimeStatsEntry statistics;
    } HandlerMapEntry;

    typedef struct {
        std::function<void(const std::string&, ValuePtr)> handler;
        msg::RuntimeStatsEntry statistics;
    } ValueHandlerMapEntry;

    static void initStats(msg::RuntimeStatsEntry& stats);

    void calcStats(msg::RuntimeStatsEntry& stats, const std::string& topic, std::chrono::high_resolution_clock::time_point start);

    void traceTriggerHandlerExec(const std::chrono::high_resolution_clock::time_point& start,
                                 const std::chrono::high_resolution_clock::time_point& end,
                                 const HandlerMapEntry& triggerHandler);

    void tracePortTriggerHandlerExec(const std::chrono::high_resolution_clock::time_point& start,
                                     const std::chrono::high_resolution_clock::time_point& end,
                                     const PortTriggerHandler& handler);

    void logControlUpdate();

    /**
     * @brief Actually sets the scheduling policy
     *
     * @note This function can only be called after the component thread is active, otherwise the
     * thread handle is invalid.
     */
    void setSchedulingPolicy();

    std::string fName;
    std::string fConfigName;
    std::vector<std::string> fConfigDirs;
    std::string fInstanceName;
    std::string fConfigOutPortTopic;
    std::string fConfigInPortTopic;
    std::thread fThread;
    // The pthread handle to the thread, populated when the component thread becomes active
    pthread_t fThreadHandle;
    // The component thread scheduling parameters
    std::atomic<SchedulingParameters> fThreadSchedulingParameters;
    std::atomic<bool> fRunRequest;
    std::atomic<bool> fStopRequest;
    std::atomic<IComponent::StateType> fState;
    std::shared_ptr<Trigger> fTrigger;
    std::vector<HandlerMapEntry> fTriggerHandlers;
    std::vector<std::shared_ptr<PortTriggerHandler>> fPortTriggerHandlers;

    std::shared_ptr<ComponentTraceEventGenerator> fComponentTraceEventGenerator;

    SenderPort<msg::LogMessage> fLogMessagePort;
    ReceiverPort<msg::LogControl> fLogControlPort;
    SenderPort<msg::String> fConfigOutPort;
    ReceiverPort<msg::String> fConfigInPort;

    std::shared_ptr<IidGenerator> fIdGenerator = nullptr;
    ValueFactory fValueFactory;

    msg::RuntimeStats fRuntimeStats;

    /**
     * The component configuration (or null, if not yet obtained)
     */
    std::unique_ptr<Json::Value> fConfig;

    ComponentLogger fComponentLogger;
};

}  // namespace mcf

#endif

