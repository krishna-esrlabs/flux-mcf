/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_COMPONENTTRACECONTROLLER_H
#define MCF_COMPONENTTRACECONTROLLER_H

#include <atomic>
#include <memory>
#include <string>

namespace mcf
{

class Value;

class GenericSenderPort;

class ComponentTraceEventGenerator;
class ValueStore;


/**
 * Controller of trace event generation on value store with given topic
 */
class ComponentTraceController
{
public:

    static constexpr const char *DEFAULT_TRACE_EVENTS_TOPIC = "/mcf/trace_events";

    explicit ComponentTraceController(std::string traceId,
                                      ValueStore &valueStore,
                                      std::string topic = DEFAULT_TRACE_EVENTS_TOPIC);

    /**
     * Enable or disable event tracing globally
     * @param onOff
     */
    void enableTrace(bool onOff)  // initial state: off
    { fIsTraceEnabled = onOff; }

    /**
     * Check if event tracing is enabled
     * @return
     */
    bool isTraceEnabled() const
    { return fIsTraceEnabled; }

    /**
     * Get the value store used by this trace controller
     */
    ValueStore& getValueStore()
    { return fValueStore; }

    /**
     * Create a new event generator instance
     *
     * @param name event generator instance name
     */
    std::unique_ptr<ComponentTraceEventGenerator> createEventGenerator(const std::string &name);

    /**
     * Set thread-local event generator or nullptr
     * (to be used with event generation macros)
     */
    static void setLocalEventGenerator(
            const std::shared_ptr<ComponentTraceEventGenerator> &eventGenerator);

    /**
     * Get thread-local trace event generator or nullptr
     */
    static std::shared_ptr<ComponentTraceEventGenerator> getLocalEventGenerator();

private:

    std::string fTraceId;
    ValueStore &fValueStore;
    const std::string fTopic;
    std::atomic_bool fIsTraceEnabled;
};

} // namespace mcf

#endif  // MCF_COMPONENTTRACECONTROLLER_H
