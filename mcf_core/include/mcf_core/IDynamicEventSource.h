/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_IDYNAMICEVENTSOURCE_H_
#define MCF_IDYNAMICEVENTSOURCE_H_

#include "mcf_core/ValueStore.h"
#include <chrono>
#include <string>

namespace mcf
{
class TimestampType;

/**
 * Interface for event sources with events possibly appearing or disappearing in the queue
 */
class IDynamicEventSource
{
    public:

        virtual ~IDynamicEventSource() = default;

        /**
         * Find next event timestamp and topic name
         *
         * This method may be called an arbitrary number of times in between subsequent calls
         * of fireEvent() or dropEvent() and shall always output the time and topic of the currently
         * next event that will be fired when fireEvent() is called. An event source may publish 
         * different events on different topics, with each individual event being published on a 
         * single topic.
         * 
         * @return true if there is currently an available event with valid timestamp and topic. False 
         *         if source does not currently have any available events. If false is returned, the event
         *         source can still provide events in the future, until isFinished() is false.  
         */
        virtual bool getNextEventInfo(TimestampType &nextEventTimestamp, std::string &nextEventTopic) = 0;

        /**
         * Fire event
         *
         * This method shall fire the event that corresponds to the time reported by the last call
         * of getNextEventInfo(). If a new event earlier than that time would be available, the new
         * event shall be ignored. If the event corresponding to the reported time is no longer
         * available in the queue, fireEvent() shall be a no-op.
         */
        virtual void fireEvent() = 0;

        /**
         * Drop event
         *
         * Implementation of this method is optional. If implemented, it shall return true and
         * behave like fireEvent except that the respective event shall be dropped.
         * If not implemented, it shall be a no-op and return false.
         *
         * If an event source returns false from this method, clients must call fireEvent()
         * instead.
         *
         * @return true,  if implemented
         *         false, if not implemented
         */
        virtual bool dropEvent()
        { return false; }

        /**
         * Returns true if the event source has no further events to publish.
         */
        virtual bool isFinished() = 0;
};

}      // namespace mcf

#endif /* MCF_IDYNAMICEVENTSOURCE_H_*/
