/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/QueuedEventSource.h"
#include "mcf_core/EventTimingController.h"
#include "mcf_core/ComponentTraceEventGenerator.h"
#include "mcf_core/ComponentTraceController.h"
#include "mcf_core/ErrorMacros.h"
#include <chrono>

namespace mcf {

using Microseconds = std::chrono::microseconds;

QueuedEventSource::QueuedEventSource(ValueStore& valueStore,
                                     std::weak_ptr<mcf::IEventTimingController> eventTimingController)
    : fValueStore(valueStore)
    , fEventTimingController(eventTimingController)
{
}

QueuedEventSource::~QueuedEventSource() = default;

bool QueuedEventSource::getNextEventInfo(TimestampType &nextEventTimestamp, std::string &nextEventTopic)
{
    IntTimestamp intTimestamp = 0;
    bool hasEvent = getNextQueuedEventInfo(intTimestamp, nextEventTopic);
    if (hasEvent)
    {
        nextEventTimestamp = TimestampType(intTimestamp);
    }

    return hasEvent;
}

void QueuedEventSource::fireEvent()
{
    fireQueuedEvent();
}

bool QueuedEventSource::isFinished()
{
    std::lock_guard<std::mutex> lock(fEventQueueMutex);
    return fIsEventSourceFinished;
}

void QueuedEventSource::clearEventQueue()
{
    std::lock_guard<std::mutex> lock(fEventQueueMutex);
    fEventQueue.clear();
}

void QueuedEventSource::pushNewEvent(
    TimestampType timestamp, 
    std::string topic, 
    ValuePtr value, 
    std::string component, 
    std::string port)
{
    std::unique_lock<std::mutex> lock(fEventQueueMutex);
    
    ValueTopicTuple<Value> valuetopicTuple {value, topic};
    fEventQueue.insert({static_cast<IntTimestamp>(timestamp),
                        ValueTopicComponentPortTuple<Value> {valuetopicTuple, component, port}});
    lock.unlock();

    auto eventTimingControllerSharedPtr = fEventTimingController.lock();
    MCF_ASSERT(eventTimingControllerSharedPtr, "EventTimingController no longer exists.");
    
    eventTimingControllerSharedPtr->triggerNewEventPushed(this);
}

void QueuedEventSource::getEventQueueInfo(std::size_t& queueSize, IntTimestamp& firstTime, IntTimestamp& lastTime) const
{
    std::lock_guard<std::mutex> lock(fEventQueueMutex);
    queueSize = fEventQueue.size();

    if (fEventQueue.empty())
    {
        firstTime = 0UL;
        lastTime = 0UL;
    }
    else
    {
        firstTime = std::get<0>(*fEventQueue.begin());
        lastTime = std::get<0>(*fEventQueue.rbegin());
    }
}

bool QueuedEventSource::getNextQueuedEventInfo(IntTimestamp &nextEventTimestamp, std::string &nextEventTopic) const
{
    std::lock_guard<std::mutex> lock(fEventQueueMutex);

    if (fEventQueue.empty())
    {
        return false;
    }

    nextEventTimestamp = std::get<0>(*fEventQueue.begin());
    const auto& nextEvent = std::get<1>(*fEventQueue.begin());
    nextEventTopic = std::get<1>(nextEvent);

    return true;
}

bool QueuedEventSource::fireQueuedEvent()
{
    std::shared_ptr<const Value> nextValue;
    std::string nextTopic;
    std::string nextEventComponent;
    std::string nextEventPort;
    {
        std::lock_guard<std::mutex> lock(fEventQueueMutex);
        if (fEventQueue.empty())
        {
            return false;
        }

        const auto& nextEvent = std::get<1>(*fEventQueue.begin());
        const auto& nextEventValueTopic = std::get<0>(nextEvent);
        
        nextTopic = std::get<1>(nextEventValueTopic);
        nextValue = std::get<0>(nextEventValueTopic);
        nextEventComponent = std::get<1>(nextEvent);
        nextEventPort = std::get<2>(nextEvent);

        fEventQueue.erase(fEventQueue.begin());
    }

    if (fComponentTraceEventGenerator)
    {
        fComponentTraceEventGenerator->traceSetQueuedEventValue(
            nextTopic,
            true,
            std::vector<uint64_t>(),
            nextValue.get(),
            nextEventComponent,
            nextEventPort);
    }

    fValueStore.setValue(nextTopic, nextValue);
    return true;
}

bool QueuedEventSource::seekQueuedEvent(IntTimestamp nextEventTimestamp)
{
    std::lock_guard<std::mutex> lock(fEventQueueMutex);
    if (fEventQueue.empty())
    {
        return false;
    }

    size_t sizeBefore = fEventQueue.size();

    // find first element >= given time stamp
    auto firstToKeep = fEventQueue.lower_bound(nextEventTimestamp);
    fEventQueue.erase(fEventQueue.begin(), firstToKeep);

    size_t sizeAfter = fEventQueue.size();

    return (sizeBefore > sizeAfter);
}

void QueuedEventSource::setEventSourceFinished(bool eventSourceFinished)
{
    std::lock_guard<std::mutex> lock(fEventQueueMutex);
    fIsEventSourceFinished = eventSourceFinished;
}

void QueuedEventSource::useTraceEventGenerator(ComponentTraceController* componentTraceController)
{
    if (componentTraceController != nullptr)
    {
        fComponentTraceEventGenerator = componentTraceController->createEventGenerator("QueuedEventSource");
    }
}

} // namespace mcf
