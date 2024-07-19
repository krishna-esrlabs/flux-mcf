/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_QUEUEDEVENTSOURCE_H_
#define MCF_QUEUEDEVENTSOURCE_H_

#include "mcf_core/IDynamicEventSource.h"
#include "mcf_core/ValueStore.h"
#include <mutex>
#include <map>
#include <tuple>

namespace mcf {

class IEventTimingController;
class ComponentTraceController;
class ComponentTraceEventGenerator;

/**
 * Event source which manages a dynamic queue of events
 */
class QueuedEventSource : public IDynamicEventSource
{
public:

    using IntTimestamp = uint64_t;  // Unix time in microsecs

    /**
     * Constructor
     *
     * @param valueStore            The value store which will be written directly to by the event 
     *                              source (A reference will be stored, the user must keep that 
     *                              object alive)
     * @param eventTimingController The eventTimingController which must be notified when a new 
     *                              event is pushed.
     */
    explicit QueuedEventSource(ValueStore& valueStore,
                               std::weak_ptr<mcf::IEventTimingController> eventTimingController);

    ~QueuedEventSource() override;

    /**
     * See base class documentation
     */
    bool getNextEventInfo(TimestampType &nextEventTimestamp, std::string &nextEventTopic) override;

    /**
     * See base class documentation
     */
    void fireEvent() override;

    /**
     * See base class documentation
     */
    bool isFinished() override;

    /**
     * Pushes a new event into the event source queue.
     */
    void pushNewEvent(TimestampType timestamp,
                      std::string topic,
                      ValuePtr value,
                      std::string component="",
                      std::string port="");

    /**
     * Clears any events currently in the event queue.
     */
    void clearEventQueue();

    /**
     * Seek for certain timestamp and drop all queued events before that.
     * If no exact timestamp is found the closest next one is chosen.
     *
     * @return true if the timestamp is found and the timeshift succeeded
     */
    bool seekQueuedEvent(IntTimestamp nextEventTimestamp);

    /**
     * Indicates whether the event source cannot produce any more events.
     */
    void setEventSourceFinished(bool eventSourceFinished);

    /**
     * Returns information about the event queue.
     */
    void getEventQueueInfo(std::size_t& size, IntTimestamp& firstTime, IntTimestamp& lastTime) const;

    /**
     * Creates an event generator from the component trace controller and writes trace events when
     * a value is written to the value store.
     */
    void useTraceEventGenerator(ComponentTraceController* ComponentTraceController);

private:

    template<typename T>
    using ValueTopicComponentPortTuple = std::tuple<ValueTopicTuple<T>, std::string, std::string>;

    /**
     * Find next event timestamp and topic
     * @return true if valid timestamp returned and false if there is currently no remaining events
     */
    bool getNextQueuedEventInfo(IntTimestamp &nextEventTimestamp, std::string &nextEventTopic) const;

    /**
     * Fire the next queued event,
     * i.e. place its value on the value store and remove it from the queue
     *
     * @return true if event succesfully fired and false if not
     */
    bool fireQueuedEvent();

    /**
     * Reference to the value store which this event source writes to directly.
     */
    ValueStore& fValueStore;

    /**
     * Mutex which protects the writing and reading of the event queue.
     */
    mutable std::mutex fEventQueueMutex;

    /**
     * Event queue which can contain multiple entries for a single timestamp.
     */
    std::multimap<IntTimestamp, ValueTopicComponentPortTuple<Value>> fEventQueue;
    
    /**
     * Bool value which stores whether the event source has any more events to publish.
     */
    bool fIsEventSourceFinished = false;

    /**
     * Event timing controller.
     */
    std::weak_ptr<mcf::IEventTimingController> fEventTimingController;

    /**
     * Trace event generator which is used to write trace events when a value is written to the 
     * value store.
     */
    std::unique_ptr<ComponentTraceEventGenerator> fComponentTraceEventGenerator;
};

} // namespace mcf

#endif // MCF_QUEUEDEVENTSOURCE_H_
