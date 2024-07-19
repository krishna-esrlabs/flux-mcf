/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/ComponentTraceEventGenerator.h"
#include "mcf_core/PortTriggerHandler.h"
#include "mcf_core/ValueStore.h"

#include <chrono>
#include <functional>
#include <memory>

namespace mcf {

PortTriggerHandler::PortTriggerHandler(std::function<void()> func,
                                       std::string name,
                                       const std::shared_ptr<ComponentTraceEventGenerator>& eventGenerator)
: fFunc(std::move(func))
, fEventFlag(std::make_shared<EventFlag>())
, fName(std::move(name))
{
    // if we have a valid event generator:
    // create TriggerTracer and register it to our event flag so as to trace activations
    if (eventGenerator) {
        fTriggerTracer = std::make_shared<TriggerTracer>(fEventFlag, eventGenerator);
        fEventFlag->addTrigger(fTriggerTracer);
    }
}

PortTriggerHandler::TriggerTracer::TriggerTracer(std::shared_ptr<EventFlag> eventFlag,
                                                 std::shared_ptr<ComponentTraceEventGenerator> eventGenerator)
: fEventFlag(std::move(eventFlag))
, fTraceEventGenerator(std::move(eventGenerator))
{}

void PortTriggerHandler::TriggerTracer::trigger()
{
    std::chrono::system_clock::time_point triggerTime;
    std::string triggerTopic;
    fEventFlag->getLastTriggerUnlocked(&triggerTime, &triggerTopic);
    fTraceEventGenerator->tracePortTriggerActivation(triggerTime, triggerTopic);
}

} // namespace mcf

