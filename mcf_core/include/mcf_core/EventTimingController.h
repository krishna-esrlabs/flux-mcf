/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_EVENTTIMINGCONTROLLER_H_
#define MCF_EVENTTIMINGCONTROLLER_H_

#include "mcf_core/IEventTimingController.h"
#include "mcf_core/TimestampType.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>
#include <thread>
#include <condition_variable>
#include <functional>

namespace mcf
{
class ReplayEventController;
class ISimpleEventSource;
class IDynamicEventSource;

class EventTimingController : public IEventTimingController
{

    struct EventSourceControl
    {
        std::string eventSourceName;
        std::shared_ptr<IDynamicEventSource> eventSource;
    };

    public:

        /**
         * Constructors
         * 
         * @param speed                 Factor for playback speed. i.e. speedFactor of 0.5 will 
         *                              playback replay at half speed.
         * 
         * @param nextEventCallback     Callback function which will be called when we find the next 
         *                              event to fire. This will be called once everytime an event 
         *                              is fired and also if a new event is pushed with triggerNewEventPushed.
         * 
         */
        explicit EventTimingController(float speed=1.f, ReplayEventController* replayEventController=nullptr);

        explicit EventTimingController(std::function<void(const std::string&, const std::string&)> nextEventCallback, 
                                       float speed=1.f, ReplayEventController* replayEventController=nullptr);

        ~EventTimingController();
        
        /**
         * Add a simple event source to list of sources that are sampled for events
         *
         * @note: The source will be wrapped by an interface wrapper to make it compatible
         *        with the interface IDynamicEventSource
         *
         * @param eventSource   The event source
         * @param eventName     A name for the event source
         */
        void addEventSource(const std::shared_ptr<ISimpleEventSource>& eventSource,
                            const std::string& eventName);

        /**
         * Add a dynamic event source to list of sources that are sampled for events
         *
         * @param eventSource   The event source
         * @param eventName     A name for the event source
         */
        void addEventSource(const std::shared_ptr<IDynamicEventSource>& eventSource,
                            const std::string& eventName);

        /**
         * Remove an event source from the list of sources that are sampled for events.
         *
         * @param eventName     A name for the event source
         */
        void removeEventSource(const std::string& eventName);

        /**
         * Set speed of internal controller time
         */
        void setSpeed(float speed);

        /**
         * Check if next event is from a specific source.
         */
        bool isNextEvent(const std::string& nextEventName, const std::string& nextEventTopic);

        /**
         * Start internal controller time running
         */
        void start();

        /**
         * Resume internal controller time
         */
        void resume();

        /**
         * Pause internal controller time
         */
        void pause();

        /**
         * Enters state which waits for a new event to be pushed with triggerNewEventPushed().
         * Can be exit this state with endWaitForPushEvent().
         */
        void beginWaitForPushEvent();

        /**
         * Exit state which waits for new event to be pushed with triggerNewEventPushed().
         */
        void endWaitForPushEvent();

        /**
         * Blocking method which waits until EventTimingController has finished.
         */
        void waitTillFinished() const;

        /**
         * Blocking method which waits until EventTimingController has initialised.
         */
        void waitTillInitialised() const;

        /**
         * Finish playback
         */
        void finish();

        /**
         * Returns true if playback has finished due to finish() being called or running out
         * of events.
         */
        bool isFinished() const;

        /**
         * Get the current internal time of the controller (in microsecs since Epoch)
         * 
         * Returns false if simulation time has not been initialised.
         */
        bool getTime(TimestampType& simulationTime) const;

        /**
         * Get the playback speed.
         */
        float getPlaybackSpeed() const;

        /**
         * Function which indicates that a new event has been added to the input eventSource. 
         * If the new event is newer than the current next event source, it will be fired first.
         */
        void triggerNewEventPushed(IDynamicEventSource* eventSource) override;

    private:

        /**
         * Lock-free implementation of triggerNewEventPushed (fFireMutex should be locked by caller)
         */
        void triggerNewEventPushedImpl(IDynamicEventSource* pushedEventSource);

        /**
         * Get the current internal time of the controller.
         * 
         * Returns false if simulation time has not been initialised.
         */
        bool getTimeImpl(TimestampType& simulationTime) const;

        /**
         * Get starting times from sources and adjust start time accordingly
         */
        void initialiseSources();

        /**
         * Returns true if all event sources have been marked as finished.
         */
        bool allEventSourcesFinished() const;

        /**
         * Return the next event to send and its sending time (thread-safe)
         */
        EventSourceControl* findNextEventSource(TimestampType &nextEventSendingTime, std::string &nextEventTopic);

        /**
         * Lock-free implementation of findNextEventSource (fFireMutex should be locked by caller)
         */
        EventSourceControl* findNextEventSourceImpl(TimestampType &nextEventSendingTime, std::string &nextEventTopic);

        /**
         * Lock-free implementation of finish() (fFireMutex should be locked by caller)
         */
        void finishImpl();

        /**
         * Lock-free implementation of resume() (fFireMutex should be locked by caller)
         */
        void resumeImpl();

        /**
         * Lock-free implementation of pause() (fFireMutex should be locked by caller)
         */
        void pauseImpl();

        /**
         * Lock-free implementation of beginWaitForPushEvent() (fFireMutex should be locked by caller)
         */
        void beginWaitForPushEventImpl();

        /**
         * Lock-free implementation of endWaitForPushEvent() (fFireMutex should be locked by caller)
         */
        void endWaitForPushEventImpl();

        /**
         * Resumes playback of simulation time. This function does not check if the simulation time
         * is currently paused or should be resumed. Use resumeImpl() and endWaitForPushEventImpl() 
         * for performing these checks before resuming.
         */
        void resumeSimTimeImpl();

        /**
         * Updates the total amount of time spent while paused.
         */
        void updateTotalPauseTime();

        /**
         * Updates the total run time.
         */
        void updateTotalRunTime();

        /**
         * Calculate the time spent paused since the last time we called updateTotalPauseTime(). If 
         * not currently paused, returns 0.
         */
        std::chrono::microseconds calcCurrentPauseTime() const;

        /**
         * Calculate the run time since we last called updateTotalRunTime().
         */
        std::chrono::microseconds calcCurrentRunTime() const;

        /**
         * Thread which fires events at the correct time until there are no more events or playback
         * is manually finished.
         */ 
        void eventProcessing();
        
        TimestampType fSimulationStartTime;
        TimestampType fPreviousRunningStartTime;
        TimestampType fPauseStartTime;

        std::chrono::microseconds fRunTimeElapsed;
        std::chrono::microseconds fPauseTimeElapsed;

        float fSpeed;                        // relative speed of internal controller time
        bool fPaused              = false;   // flag determining if we are in paused state
        bool fWaitForPushEvent    = false;   // flag indicating if ETC should wait for a new event to be pushed.
        bool fIsInitialised       = false;   // flag determining whether event sources have been initialised
        bool fShouldCheckNextEvent = false;  // flag indicating whether we should re-check the next event
        bool fEnd                 = false;   // flag determining whether we have reached end of recording while in pause mode

        std::vector<EventSourceControl> fEventSources;

        /**
         * Time of the current next event to be fired.
         */
        TimestampType fNextEventTime;

        /**
         * Mutex so that only one firing event can be called at any one time
         */
        mutable std::mutex fFireMutex;

        /**
         * Condition variable which waits when EventTimingController is paused or waiting for a push event.
         */
        std::condition_variable fFireCondVar;

        /**
         * Condition variable which waits for EventTimingController to finish
         */
        mutable std::condition_variable fFinishCondVar;

        /** 
         * Condition variable which waits until the EventTimingController has been initialised.
         */
        mutable std::condition_variable fInitCondVar;

        /**
         * Thread object for main event processing thread which runs until there are no 
         * more events or playback is manually finished.
         */
        std::unique_ptr<std::thread> fEventProcessingThread;

        /**
         * Callback function which is called when we find the next event to fire. This will be called once everytime
         * an event is fired and also if a new event is pushed with triggerNewEventPushed.
         */
        std::function<void(const std::string&, const std::string&)> fNextEventCallback;

        /**
         * Pointer to replay event controller.
         */
        ReplayEventController* fReplayEventController;

};

}      // namespace mcf

#endif /* MCF_EVENTTIMINGCONTROLLER_H_*/
