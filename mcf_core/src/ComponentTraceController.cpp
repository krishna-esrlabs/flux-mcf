/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/ComponentTraceController.h"


#include "mcf_core/LoggingMacros.h"
#include "mcf_core/Messages.h"
#include "mcf_core/Port.h"
#include "mcf_core/ValueStore.h"

namespace mcf {

/**
 * Thread-local component trace event generator
 */
extern thread_local std::shared_ptr<ComponentTraceEventGenerator> gComponentTraceEventGenerator;

ComponentTraceController::ComponentTraceController(std::string traceId,
                                                   ValueStore& valueStore,
                                                   std::string topic)
: fTraceId(std::move(traceId))
, fValueStore(valueStore)
, fTopic(std::move(topic))
, fIsTraceEnabled(false)
{
}

std::unique_ptr<ComponentTraceEventGenerator> ComponentTraceController::createEventGenerator(
        const std::string &name)
{
    return std::make_unique<ComponentTraceEventGenerator>(fTraceId, name, *this, fValueStore, fTopic);
}

void ComponentTraceController::setLocalEventGenerator(
        const std::shared_ptr<ComponentTraceEventGenerator>& eventGenerator)
{
    gComponentTraceEventGenerator = eventGenerator;
}

std::shared_ptr<ComponentTraceEventGenerator> ComponentTraceController::getLocalEventGenerator()
{
    return gComponentTraceEventGenerator;
}

}