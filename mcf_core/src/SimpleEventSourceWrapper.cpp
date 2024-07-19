/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/SimpleEventSourceWrapper.h"

#include <chrono>
#include <memory>

namespace mcf
{

/*
 * Constructor
 */
SimpleEventSourceWrapper::SimpleEventSourceWrapper(
        std::shared_ptr<ISimpleEventSource> eventSource)
: fEventSource(std::move(eventSource))
{}

bool SimpleEventSourceWrapper::getNextEventInfo(TimestampType &nextEventTimestamp, std::string &nextEventTopic)
{
    // if we do not yet have a valid time in the cache, query underlying event source
    if (!fIsCached)
    {
        fHasEvent = fEventSource->getNextEventTime(fNextEventTimestamp);
        fIsCached = true;
    }

    // output latest cached results obtained from underlying event source
    nextEventTimestamp = fNextEventTimestamp;
    nextEventTopic = "";
    
    return fHasEvent;
}


void SimpleEventSourceWrapper::fireEvent()
{
    // if we do not have a valid time in the cache, getNextEventTime() of the event source
    // has not yet been called since last execution of fireEvent().
    // => call getNextEventTime() now
    if(!fIsCached)
    {
        TimestampType dummy;
        fEventSource->getNextEventTime(dummy);
    }

    // remember that a new event time needs to be obtained and cached
    fIsCached = false;

    // call event source
    fEventSource->fireEvent();
}


bool SimpleEventSourceWrapper::isFinished()
{
    // If we do not have a valid time in the cache, it means that we haven't called getNextInfo() yet. We
    // need to call it to check if there are any events remaining.
    if (!fIsCached)
    {
        TimestampType dummyTimestamp;
        std::string dummyEventTopic;
        fHasEvent = getNextEventInfo(dummyTimestamp, dummyEventTopic);
    }

    return !fHasEvent;
}

}      // namespace mcf
