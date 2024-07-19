/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_COMPONENTTRACEMESSAGES_H
#define MCF_COMPONENTTRACEMESSAGES_H

#include "mcf_core/Value.h"

#include <msgpack.hpp>

namespace mcf {

namespace msg {

class PortDescriptor : public Value {
public:

    std::string name;   // fully qualified name
    std::string topic;
    bool connected = false;

    MSGPACK_DEFINE(name, topic, connected)
};

class TriggerDescriptor : public Value {
public:

    std::string topic;
    uint64_t time = 0UL;       // time of trigger event in microseconds since 1970

    MSGPACK_DEFINE(topic, time)
};

/**
 * Base class for component trace event messages.
 */
class ComponentTraceEvent : public Value {
public:
    ~ComponentTraceEvent() override = default;

    std::string traceId;       // ID of the trace (e.g. host ID)
    uint64_t time = 0UL;       // time of event in microseconds since 1970
    std::string componentName;
    int threadId;
    int cpuId;
};

/**
 * Value holding a port_write component trace event
 */
class ComponentTracePortWrite : public ComponentTraceEvent {
public:

    PortDescriptor portDescriptor;
    uint64_t valueId;          // ID of value written to port
    std::vector<uint64_t> inputValueIds;    // IDs of input values to component

    MSGPACK_DEFINE(traceId, time, componentName, portDescriptor, valueId, inputValueIds, threadId, cpuId)
};

/**
 * Value holding a port_peek component trace event
 */
class ComponentTracePortPeek : public ComponentTraceEvent {
public:

    PortDescriptor portDescriptor;
    uint64_t valueId;          // ID of value read from port, but not removed from its queue

    MSGPACK_DEFINE(traceId, time, componentName, portDescriptor, valueId, threadId, cpuId)
};

/**
 * Value holding a port_read component trace event
 */
class ComponentTracePortRead : public ComponentTraceEvent {
public:

    PortDescriptor portDescriptor;
    uint64_t valueId;          // ID of value read from port

    MSGPACK_DEFINE(traceId, time, componentName, portDescriptor, valueId, threadId, cpuId)
};

/**
 * Value holding an execution time trace event
 */
class ComponentTraceExecTime : public ComponentTraceEvent {
public:

    std::string description;   // description (or name) of the executed piece of code
    float executionTime = 0.f; // duration of execution in seconds. (Field 'time' is end time.)

    MSGPACK_DEFINE(traceId, time, componentName, description, executionTime, threadId, cpuId)
};

/**
 * Value holding a remote value transfer event
 */
class ComponentTraceRemoteTransferTime : public ComponentTraceEvent {
public:

    std::string description;   // description (or name) of the executed piece of code
    float executionTime = 0.f; // duration of execution in seconds. (Field 'time' is end time.)

    MSGPACK_DEFINE(traceId, time, componentName, description, executionTime, threadId, cpuId)
};

/**
 * Value holding a trigger activation trace event
 */
class ComponentTracePortTriggerActivation : public ComponentTraceEvent {
public:

    TriggerDescriptor triggerDescriptor;

    MSGPACK_DEFINE(traceId, time, componentName, triggerDescriptor, threadId, cpuId)
};

/**
 * Value holding a trigger execution trace event
 */
class ComponentTracePortTriggerExec : public ComponentTraceEvent {
public:

    TriggerDescriptor triggerDescriptor;
    std::string handlerName;   // name of the executed trigger handler
    float executionTime = 0.f; // duration of execution time event in seconds. (Field 'time' is end time.)

    MSGPACK_DEFINE(traceId, time, componentName, triggerDescriptor, handlerName, executionTime, threadId, cpuId)
};

/**
 * Value holding a program flow trace event
 */
class ComponentTraceProgramFlowEvent : public ComponentTraceEvent {
public:

    std::string eventName;
    std::vector<uint64_t> inputValueIds;    // IDs of input values to component

    MSGPACK_DEFINE(traceId, time, componentName, eventName, inputValueIds, threadId, cpuId)
};

template<typename T>
inline void registerComponentTraceValueTypes(T& r) {
    r.template registerType<ComponentTracePortWrite>("mcf::ComponentTracePortWrite");
    r.template registerType<ComponentTracePortPeek>("mcf::ComponentTracePortPeek");
    r.template registerType<ComponentTracePortRead>("mcf::ComponentTracePortRead");
    r.template registerType<ComponentTraceExecTime>("mcf::ComponentTraceExecTime");
    r.template registerType<ComponentTraceRemoteTransferTime>("mcf::ComponentTraceRemoteTransferTime");
    r.template registerType<ComponentTracePortTriggerActivation>("mcf::ComponentTracePortTriggerActivation");
    r.template registerType<ComponentTracePortTriggerExec>("mcf::ComponentTracePortTriggerExec");
    r.template registerType<ComponentTraceProgramFlowEvent>("mcf::ComponentTraceProgramFlowEvent");
}

} // namespace msg
} // namespace mcf

#endif // MCF_COMPONENTTRACEMESSAGES_H
