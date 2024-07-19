/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_ICOMPONENT_H
#define MCF_ICOMPONENT_H

#include <string>
#include <memory>

#include "mcf_core/Messages.h"
#include "mcf_core/ComponentLogger.h"

#include "json/forwards.h"

namespace mcf {

class ComponentTraceEventGenerator;
class GenericReceiverPort;
class IComponentConfig;
class IidGenerator;
class PortTriggerHandler;
class Port;
class ValueFactory;

class IComponent {
public:
    virtual ~IComponent() = default;

    // component control interface
    // to be used by ComponentManager
    virtual void ctrlSetConfigName(const std::string& cfgName) = 0;
    virtual void ctrlSetConfigDirs(const std::vector<std::string>& cfgDirs) = 0;
    virtual void ctrlConfigure(IComponentConfig& config) = 0;
    virtual void ctrlStart() = 0;
    virtual void ctrlRun() = 0;
    virtual void ctrlStop() = 0;

    /**
     * Set the component trace event generator
     *
     * @param eventGenerator  pointer to the event generator (may be nullptr for none)
     */
    virtual void ctrlSetComponentTraceEventGenerator(
            const std::shared_ptr<mcf::ComponentTraceEventGenerator>& eventGenerator) = 0;

    /**
     * @brief An enum of supported scheduling policies for component threads
     *
     * Represents a sensible subset of POSIX thread priorities as an C++ enum. The values correspond
     * to those defined in <pthread.h>. Currently, FIFO and round-robin are supported as real-time
     * priorities, Other is the non-realtime best-effort policy of the OS.
     *
     * @sa sched(7)
     */
    enum SchedulingPolicy
    {
        /// dynamic scheduling at OS' best effort
        Other = SCHED_OTHER,
        /// FIFO, run until preempted by a higher priority or blocked
        Fifo = SCHED_FIFO,
        /// same as FIFO but with time slices
        RoundRobin = SCHED_RR,
        /// Special value to represent no change in the current policy
        Default = 1 + SCHED_OTHER + SCHED_FIFO + SCHED_RR
    };
    /**
     * @brief A data structure that wraps the scheduling configuration
     * 
     * @sa IComponent::SchedulingPolicy
     */
    struct SchedulingParameters
    {
        SchedulingPolicy policy;
        /// The static scheduling priority for real-time schedulers
        int priority;
    };

    /**
     * @brief Sets the scheduling policy for the component's thread
     *
     * If the component thread is not running, the scheduling policy will be set at the component's
     * startup. Else, it will be changed immediately.
     *
     * @note This does not affect already spawned child threads of the component, and may be
     * overridden by the component itself.
     *
     * @note To use this functionality, the executable must have CAP_SYS_NICE in its set of
     * effective capabilities.
     *
     * @sa sched(7), pthread_getschedparam(3)
     *
     * @param parameters The scheduling policy and priority as defined in <pthread.h>; currently
     * only SCHED_FIFO, SCHED_RR, SCHED_OTHER are supported.
     */
    virtual void ctrlSetSchedulingParameters(const SchedulingParameters& parameters) = 0;

    /**
     * @brief Sets the logging levels for the component's logger
     *
     * The logger has two separate log levels, one for the console log sink and one for the value
     * store log sink. In most cases, the value store log level should be not below the console
     * level.
     *
     * @param consoleLevel Logging level on the console
     * @param valueStoreLevel Logging level on the value store
     */
    virtual void ctrlSetLogLevels(
        LogSeverity consoleLevel, LogSeverity valueStoreLevel) = 0;

    /**
     * @brief Inject the component logger to a thread.
     *
     * If a thread different from the component's own wants to write its log output specifically via
     * the component's logging interface, it needs to call this method first. The typical use case
     * is a worker thread/thread pool inside of a component. After a call to this function, the
     * logging macros (such as MCF_INFO) will produce output in the component's own log (both in the
     * value store and on the console).
     */
    virtual void injectLocalLogger() = 0;
    virtual LogSeverity getConsoleLogLevel() const = 0;
    virtual LogSeverity getValueStoreLogLevel() const = 0;

    virtual std::shared_ptr<ComponentTraceEventGenerator> getComponentTraceEventGenerator() const = 0;

    virtual std::string getName() const = 0;
    virtual std::string getConfigName() const = 0;
    virtual std::string getConfigDir() const = 0;
    virtual msg::RuntimeStats getStatistics() const = 0;
    virtual const Json::Value& getConfig() = 0;
    virtual void readConfig() = 0;

    typedef enum {
        INIT,
        STARTING_UP,
        STARTED,
        RUNNING,
        SHUTTING_DOWN,
        WAIT_STOP,
        STOPPED
    } StateType;

    virtual StateType getState() const = 0;

    virtual void setIdGenerator(std::shared_ptr<IidGenerator> idGenerator) = 0;

    virtual const IidGenerator& idGenerator() const = 0;
    virtual const ValueFactory& valueFactory() const = 0;


protected:
    friend GenericReceiverPort;

    virtual void registerHandler(std::shared_ptr<PortTriggerHandler> handler) = 0;
    virtual void unregisterHandler(std::shared_ptr<PortTriggerHandler> handler) = 0;
};

} // namespace mcf

#endif // MCF_ICOMPONENT_H

