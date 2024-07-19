/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/ComponentTraceEventGenerator.h"

#include "mcf_core/ComponentTraceController.h"
#include "mcf_core/LoggingMacros.h"
#include "mcf_core/Messages.h"
#include "mcf_core/Port.h"
#include "mcf_core/ValueStore.h"
#include <unistd.h>
#include <sys/syscall.h>

namespace mcf {

namespace {

template<typename T>
inline void fillCpuId(T& event)
{
    #ifdef SYS_getcpu
    int cpuId  = 0;
    int status = syscall(SYS_getcpu, &cpuId, NULL, NULL);
    if (status != -1)
    {
        event.cpuId = cpuId;
    }
    #endif
}


inline void fillPortWriteEvent(const std::string& traceId,
                               const std::string& compName,
                               const std::string& topic,
                               const std::vector<uint64_t>& inputIds,
                               bool isConnected,
                               const Value* vp,
                               msg::ComponentTracePortWrite& outEvent)
{
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    msg::PortDescriptor portDesc;
    portDesc.name = "unnamed";
    portDesc.topic = topic;
    portDesc.connected = isConnected;

    outEvent.traceId = traceId;
    outEvent.time = time;
    outEvent.componentName = compName;
    outEvent.portDescriptor = portDesc;
    outEvent.threadId = syscall(SYS_gettid);
    fillCpuId(outEvent);
    if (vp != nullptr)
    {
        outEvent.valueId = vp->id();
        outEvent.inputValueIds = inputIds;
    }
    else
    {
        outEvent.valueId = 0UL;
    }
}

template<typename EventValueType>
inline void fillPortReadEvent(const std::string& traceId,
                              const std::string& compName,
                              const std::string& topic,
                              bool isConnected,
                              const Value* vp,
                              EventValueType& outEvent)
{
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    msg::PortDescriptor portDesc;
    portDesc.name = "unnamed";
    portDesc.topic = topic;
    portDesc.connected = isConnected;

    outEvent.traceId = traceId;
    outEvent.time = time;
    outEvent.componentName = compName;
    outEvent.portDescriptor = portDesc;
    outEvent.threadId = syscall(SYS_gettid);
    fillCpuId(outEvent);
    if (vp != nullptr)
    {
        outEvent.valueId = vp->id();
    }
    else
    {
        outEvent.valueId = 0UL;
    }
}

}

/*
 * Constructor
 */
ComponentTraceEventGenerator::ComponentTraceEventGenerator(
        std::string traceId,
        std::string name,
        const ComponentTraceController& traceController,
        ValueStore& valueStore,
        std::string topic)
: fTraceController(traceController)
, fValueStore(valueStore)
, fTopic(std::move(topic))
, fTraceId(std::move(traceId))
, fName(std::move(name))
, fIsEnabled(true)
{
}

template <typename T>
void ComponentTraceEventGenerator::writeTraceEvent(T&& event) const
{
    fValueStore.setValue(fTopic, std::forward<T>(event));
}

void ComponentTraceEventGenerator::traceSetPortValue(
        const std::string& topic, 
        bool isConnected, 
        const std::vector<uint64_t>& inputIds, 
        const Value* value) const
{
    // do nothing if event logging disabled
    if (!isGloballyEnabled() || !isEnabled())
    {
        return;
    }

    msg::ComponentTracePortWrite event;
    fillPortWriteEvent(fTraceId, fName, topic, inputIds, isConnected, value, event);
    writeTraceEvent(std::move(event));
}

void ComponentTraceEventGenerator::traceSetQueuedEventValue(
        const std::string& topic, 
        bool isConnected, 
        const std::vector<uint64_t>& inputIds, 
        const Value* value, 
        const std::string& component, 
        const std::string& port) const
{
    // do nothing if event logging disabled
    if (!isGloballyEnabled() || !isEnabled())
    {
        return;
    }
    
    const std::string& componentName = component.empty() ? fName : component;

    msg::ComponentTracePortWrite event;
    fillPortWriteEvent(fTraceId, componentName, topic, inputIds, isConnected, value, event);
    writeTraceEvent(std::move(event));
}

void ComponentTraceEventGenerator::tracePeekPortValue(
        const std::string& topic, bool isConnected, const Value* value) const
{
    // do nothing if event logging disabled
    if (!isGloballyEnabled() || !isEnabled())
    {
        return;
    }
    msg::ComponentTracePortPeek event;
    fillPortReadEvent(fTraceId, fName, topic, isConnected, value, event);
    writeTraceEvent(std::move(event));
}

void ComponentTraceEventGenerator::traceGetPortValue(
        const std::string& topic, bool isConnected, const Value* value) const
{
    // do nothing if event logging disabled
    if (!isGloballyEnabled() || !isEnabled())
    {
        return;
    }
    msg::ComponentTracePortRead event;
    fillPortReadEvent(fTraceId, fName, topic, isConnected, value, event);
    writeTraceEvent(std::move(event));
}


void ComponentTraceEventGenerator::traceExecutionTime(
        uint64_t endTime, float duration, const std::string& name) const
{
    // do nothing if event logging disabled
    if (!isGloballyEnabled() || !isEnabled())
    {
        return;
    }

    msg::ComponentTraceExecTime event;
    event.traceId = fTraceId;
    event.time = endTime;
    event.componentName = fName;
    event.executionTime = duration;
    event.description = name;
    event.threadId = syscall(SYS_gettid);
    fillCpuId(event);

    // TODO: log ID of value
    writeTraceEvent(std::move(event));
}

void ComponentTraceEventGenerator::traceExecutionTime(
        const std::chrono::high_resolution_clock::time_point& start,
        const std::chrono::high_resolution_clock::time_point& end,
        const std::string& name = "") const
{
    // do nothing if event logging disabled
    if (!isGloballyEnabled() || !isEnabled())
    {
        return;
    }

    msg::ComponentTraceExecTime event;
    event.traceId = fTraceId;
    event.time = std::chrono::duration_cast<std::chrono::microseconds>(end.time_since_epoch()).count();
    event.componentName = fName;
    event.executionTime = (std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) * 1.e-9f;
    event.description = name;
    event.threadId = syscall(SYS_gettid);
    fillCpuId(event);

    writeTraceEvent(std::move(event));
}

void ComponentTraceEventGenerator::traceRemoteTransferTime(
        const std::chrono::high_resolution_clock::time_point& start,
        const std::chrono::high_resolution_clock::time_point& end,
        const std::string& name = "") const
{
    // do nothing if event logging disabled
    if (!isGloballyEnabled() || !isEnabled())
    {
        return;
    }

    msg::ComponentTraceRemoteTransferTime event;
    event.traceId = fTraceId;
    event.time = std::chrono::duration_cast<std::chrono::microseconds>(end.time_since_epoch()).count();
    event.componentName = fName;
    event.executionTime = (std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) * 1.e-9f;
    event.description = name;
    event.threadId = syscall(SYS_gettid);
    fillCpuId(event);
    
    writeTraceEvent(std::move(event));
}

void ComponentTraceEventGenerator::tracePortTriggerActivation(
        const std::chrono::high_resolution_clock::time_point &time,
        const std::string &topic) const
{
    // do nothing if event logging disabled
    if (!isGloballyEnabled() || !isEnabled())
    {
        return;
    }

    msg::TriggerDescriptor triggerDesc;
    triggerDesc.topic = topic;
    triggerDesc.time = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count();

    msg::ComponentTracePortTriggerActivation event;
    event.traceId = fTraceId;
    event.time = triggerDesc.time;
    event.componentName = fName;
    event.triggerDescriptor = triggerDesc;
    event.threadId = syscall(SYS_gettid);
    fillCpuId(event);

    writeTraceEvent(std::move(event));
}

void ComponentTraceEventGenerator::tracePortTriggerExec(const std::chrono::high_resolution_clock::time_point& start,
                                                        const std::chrono::high_resolution_clock::time_point& end,
                                                        const PortTriggerHandler& triggerHandler) const
{
    // do nothing if event logging disabled
    if (!isGloballyEnabled() || !isEnabled())
    {
        return;
    }

    msg::TriggerDescriptor triggerDesc;
    std::chrono::high_resolution_clock::time_point time;
    triggerHandler.getEventFlag()->getLastTrigger(&time, &triggerDesc.topic);
    triggerDesc.time = std::chrono::duration_cast<std::chrono::microseconds>(
            time.time_since_epoch()).count();

    msg::ComponentTracePortTriggerExec event;
    event.traceId = fTraceId;
    event.time = std::chrono::duration_cast<std::chrono::microseconds>(end.time_since_epoch()).count();
    event.componentName = fName;
    event.executionTime = (std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) * 1.e-9f;
    event.handlerName = triggerHandler.getName();
    event.triggerDescriptor = triggerDesc;
    event.threadId = syscall(SYS_gettid);
    fillCpuId(event);

    writeTraceEvent(std::move(event));
}


void ComponentTraceEventGenerator::traceProgramFlowEvent(const std::string& eventName,
                                                         const std::vector<uint64_t>& inputValueIds) const
{
    // do nothing if event logging disabled
    if (!isGloballyEnabled() || !isEnabled())
    {
        return;
    }

    auto time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    msg::ComponentTraceProgramFlowEvent event;
    event.traceId = fTraceId;
    event.time = time;
    event.componentName = fName;
    event.eventName = eventName;
    event.inputValueIds = inputValueIds;
    event.threadId = syscall(SYS_gettid);
    fillCpuId(event);

    writeTraceEvent(std::move(event));
}


/*
 * Check if trace event generation is globally enabled
 */
bool ComponentTraceEventGenerator::isGloballyEnabled() const
{
    return fTraceController.isTraceEnabled();
}

/*
 * Set thread-local trace event generator or nullptr
 */
void ComponentTraceEventGenerator::setLocalInstance(
        const std::shared_ptr<ComponentTraceEventGenerator>& eventGenerator)
{
    ComponentTraceController::setLocalEventGenerator(eventGenerator);
}

/**
 * Get thread-local event controller or nullptr
 */
std::shared_ptr<ComponentTraceEventGenerator> ComponentTraceEventGenerator::getLocalInstance()
{
    return ComponentTraceController::getLocalEventGenerator();
}

}