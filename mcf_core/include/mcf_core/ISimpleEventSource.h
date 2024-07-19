/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_ISIMPLEEVENTSOURCE_H_
#define MCF_ISIMPLEEVENTSOURCE_H_

#include "mcf_core/ValueStore.h"
#include <chrono>
#include <string>

namespace mcf
{
class TimestampType;

/**
 * Interface for simple event sources
 */
class ISimpleEventSource
{
    public:

        virtual ~ISimpleEventSource() = default;

        /**
         * Find next event timestamp
         *
         * This method shall be called exactly once between subsequent calls of fireEvent().
         * Otherwise the behaviour of the event source is undefined.
         *
         * @return true if valid timestamp returned and false if source can no longer produce events
         */
        virtual bool getNextEventTime(TimestampType &nextEventTimestamp) = 0;
        
        /**
         * Find next event timestamp and topic name
         *
         * Returns true if valid timestamp returned and false if source can no longer produce events
         */
        virtual bool getNextEventInfo(TimestampType &nextEventTimestamp, std::string &nextEventTopic)
        {
            nextEventTopic = "";
            return getNextEventTime(nextEventTimestamp);
        };
        
        /**
         * Fire event
         *
         * This method shall be called exactly once between subsequent calls of getNextEventTime().
         * Otherwise the behaviour of the event source is undefined.
         *
         * @return true if event succesfully fired and false if not
         */
        virtual bool fireEvent() = 0;
};


}      // namespace mcf

#endif /* MCF_ISIMPLEEVENTSOURCE_H_*/
