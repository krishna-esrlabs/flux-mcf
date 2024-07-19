/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_SIMPLEEVENTSOURCEWRAPPER_H_
#define MCF_SIMPLEEVENTSOURCEWRAPPER_H_

#include "mcf_core/ISimpleEventSource.h"
#include "mcf_core/IDynamicEventSource.h"
#include "mcf_core/TimestampType.h"
#include <memory>

namespace mcf
{

/**
 * Wrapper converting a simple event source into a dynamic event source
 *
 * Simple event sources require exactly one call of their interface method getNextEventTime()
 * between two calls of fireEvent().
 *
 * In order to relax that constraint, this wrapper forwards only one call of getNextEventTime()
 * to its underlying event source and caches the result to serve subsequent calls until fireEvent()
 * has been executed. In addition, the wrapper will call getNextEventTime() immediately before
 * fireEvent(), if no such call occurred before.
 */
class SimpleEventSourceWrapper : public IDynamicEventSource
{
public:

    /**
     * Constructor
     *
     * @param eventSource  the simple event source object to be wrapped
     */
    explicit SimpleEventSourceWrapper(std::shared_ptr<ISimpleEventSource> eventSource);

    /*
     * see interface documentation
     */
    bool getNextEventInfo(TimestampType &nextEventTimestamp, std::string &nextEventTopic) override;

    /*
     * see interface documentation
     */
    void fireEvent() override;

    /*
     * see interface documentation
     */
    bool isFinished() override;

private:

    /**
     * The wrapped simple event source
     */
    std::shared_ptr<ISimpleEventSource> fEventSource;

    /**
     * The cached next event time, if any
     */
    TimestampType fNextEventTimestamp;

    /**
     * The cached return value from getNextEventTime() of event source
     */
    bool fHasEvent;

    /**
     * Flag indicating whether the event time cache is valid
     */
    bool fIsCached = false;
};

}      // namespace mcf

#endif /* MCF_SIMPLEEVENTSOURCEWRAPPER_H_*/
