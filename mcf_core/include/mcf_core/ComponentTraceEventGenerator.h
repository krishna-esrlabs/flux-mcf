/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_COMPONENTTRACEEVENTGENERATOR_H
#define MCF_COMPONENTTRACEEVENTGENERATOR_H

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace mcf {

class Value;
class GenericSenderPort;
class GenericReceiverPort;
class ValueStore;

using ValuePtr = std::shared_ptr<const Value>;

class PortTriggerHandler;
class ComponentTraceController;

class ComponentTraceEventGenerator {

public:

    // TODO: hide from public access?
    ComponentTraceEventGenerator(
            std::string traceId,
            std::string name,
            const ComponentTraceController& traceController,
            ValueStore& valueStore,
            std::string topic);

    /**
     * Trace event of value being set on port
     *
     * @param topic       the topic to which the port is mapped
     * @param isconnected indicates if the port is connected at the time of the set
     * @param value       the value
     */
    void traceSetPortValue(const std::string& topic, 
                           bool isConnected, 
                           const std::vector<uint64_t>& inputIds, 
                           const Value* value) const;

    /**
     * Trace event of value being set by QueuedEventSource. Can be optionally written on behalf of 
     * a component and port.
     *
     * @param topic       the topic to which the port is mapped
     * @param isconnected indicates if the port is connected at the time of the set
     * @param value       the value
     * @param component   component name that will appear in trace event component field.  
     * @param port        port name that will appear in trace event port field.  
     */
    void traceSetQueuedEventValue(const std::string& topic, 
                                  bool isConnected, 
                                  const std::vector<uint64_t>& inputIds, 
                                  const Value* value, 
                                  const std::string& component="", 
                                  const std::string& port="") const;

    /**
     * Trace event of value being read from port but not deleted from its queue
     *
     * @param topic       the topic to which the port is mapped
     * @param isconnected indicates if the port is connected at the time of the peek
     * @param value       the value
     */
    void tracePeekPortValue(const std::string& topic, bool isConnected, const Value* value) const;

    /**
     * Trace event of value being read from port
     *
     * @param topic       the topic to which the port is mapped
     * @param isconnected indicates if the port is connected at the time of the get
     * @param value       the value
     */
    void traceGetPortValue(const std::string& topic, bool isConnected, const Value* value) const;

    /**
     * Trace event of measured execution time
     *
     * @param endTime   end point of measured interval in microseconds since 1970
     * @param duration  measured duration in seconds
     * @param name      name of the executed piece of code
     */
    void traceExecutionTime(uint64_t endTime, float duration, const std::string& name = "") const;

    /**
     * Trace event of measured execution time
     *
     * @param start     starting point of measured interval
     * @param end       end point of measured interval
     * @param name      name of the executed piece of code
     */
    void traceExecutionTime(const std::chrono::high_resolution_clock::time_point& start,
                            const std::chrono::high_resolution_clock::time_point& end,
                            const std::string& name) const;

    /**
     * Trace event of measured remote value transfer time
     *
     * @param start     starting point of measured interval
     * @param end       end point of measured interval
     * @param name      name of the executed piece of code
     */
    void traceRemoteTransferTime(const std::chrono::high_resolution_clock::time_point& start,
                                 const std::chrono::high_resolution_clock::time_point& end,
                                 const std::string& name) const;

    /**
     * Trace event of trigger handler execution
     *
     * @param start           starting time of execution
     * @param end             end time of execution
     * @param triggerHandler  the trigger handler
     */
    void tracePortTriggerExec(const std::chrono::high_resolution_clock::time_point& start,
                              const std::chrono::high_resolution_clock::time_point& end,
                              const PortTriggerHandler& triggerHandler) const;

    /**
     * Trace event of trigger activation
     *
     * @param time            time of trigger activation
     * @param topic           value store topic that caused trigger activation
     */
    void tracePortTriggerActivation(const std::chrono::high_resolution_clock::time_point& time,
                                    const std::string& topic) const;

    /**
     * Trace program flow event
     *
     * @param eventName       Name that identifies the program flow event
     * @param inputValueIds   Value id(s) of input values to component publishing event
     */
    void traceProgramFlowEvent(const std::string& eventName,
                               const std::vector<uint64_t>& inputValueIds) const;

    /**
     * Enable/disable trace event generation
     * @param onOff
     */
    void enable(bool onOff)
    { fIsEnabled = onOff; }

    /**
     * Check if trace event generation is locally enabled
     */
    bool isEnabled() const
    { return fIsEnabled; }

    /**
     * Check if trace event generation is globally enabled
     */
    bool isGloballyEnabled() const;

    /**
     * @brief Checks if a topic is a tracing topic (to avoid recursion)
     * 
     * @param topic A topic string
     * @return true if `topic` equals to the topic of the trace events
     * @return false otherwise
     */
    bool isTracingTopic(const std::string& topic) const { return fTopic == topic; }

    /**
     * Set thread-local trace event generator or nullptr
     * (to be used with tracing macros)
     */
    static void setLocalInstance(
            const std::shared_ptr<ComponentTraceEventGenerator>& eventGenerator);

    /**
     * Get thread-local trace event generator or nullptr
     */
    static std::shared_ptr<ComponentTraceEventGenerator> getLocalInstance();

private:

    /**
     * Non-virtual helper method writing the given trace event to the value store
     * @param event
     */
    template <typename T>
    void writeTraceEvent(T&& event) const;

    const ComponentTraceController& fTraceController;
    ValueStore& fValueStore;
    const std::string fTopic;
    const std::string fTraceId;
    const std::string fName;
    std::atomic_bool fIsEnabled;  // enabled by default in the constructor
};

}  // namespace mcf

#endif  // MCF_COMPONENTTRACEEVENTGENERATOR_H
