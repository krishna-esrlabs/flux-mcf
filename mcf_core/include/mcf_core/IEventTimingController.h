/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_IEVENTTIMINGCONTROLLER_H_
#define MCF_IEVENTTIMINGCONTROLLER_H_

namespace mcf {
class IDynamicEventSource;

/**
 * Event Timing Controller interface
 */
class IEventTimingController
{

public:

    virtual ~IEventTimingController() = default;
    
    /**
     * Function which should be called whenever a new event is pushed to an event source which has
     * been added to the EventTimingController.
     */
    virtual void triggerNewEventPushed(IDynamicEventSource* eventSource) = 0;
    
};

}

#endif // MCF_IEVENTTIMINGCONTROLLER_H_