/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/ErrorMacros.h"
#include "mcf_core/EventTimingController.h"
#include "mcf_core/ReplayEventController.h"
#include "mcf_core/ThreadName.h"
#include "mcf_core/SimpleEventSourceWrapper.h"
#include "mcf_core/ISimpleEventSource.h"
#include "mcf_core/IDynamicEventSource.h"

#include <algorithm>


namespace mcf
{

EventTimingController::EventTimingController(float speed, ReplayEventController* replayEventController)
    : fSpeed(speed)
    , fRunTimeElapsed(0)
    , fPauseTimeElapsed(0)
    , fNextEventTime(std::chrono::system_clock::from_time_t(0))
    , fReplayEventController(replayEventController) {}

EventTimingController::EventTimingController(std::function<void(const std::string&, const std::string&)> nextEventCallback, 
                                             float speed, ReplayEventController* replayEventController)
    : fSpeed(speed)
    , fRunTimeElapsed(0)
    , fPauseTimeElapsed(0)
    , fNextEventTime(std::chrono::system_clock::from_time_t(0))
    , fNextEventCallback(nextEventCallback)
    , fReplayEventController(replayEventController) {}

void EventTimingController::addEventSource(const std::shared_ptr<ISimpleEventSource>& eventSource,
                                           const std::string& eventName)
{
    // wrap event source and hand over as dynamic event source
    addEventSource(std::make_shared<SimpleEventSourceWrapper>(eventSource), eventName);
}

void EventTimingController::addEventSource(const std::shared_ptr<IDynamicEventSource>& eventSource,
                                           const std::string& eventName)
{
    // create control structure
    EventSourceControl newSource;
    newSource.eventSourceName = eventName;
    newSource.eventSource = eventSource;

    // save control structure
    std::lock_guard<std::mutex> lock(fFireMutex);
    fEventSources.push_back(newSource);

    // Check if event source has a new event that should be fired next.
    triggerNewEventPushedImpl(eventSource.get());
}

void EventTimingController::removeEventSource(const std::string& eventName)
{
    auto isTargetEvent = [eventName](const EventSourceControl& event) {
        return event.eventSourceName == eventName;
    };

    std::lock_guard<std::mutex> lock(fFireMutex);
    fEventSources.erase(std::remove_if(fEventSources.begin(),
                                       fEventSources.end(),
                                       isTargetEvent),
                        fEventSources.end());

    // Signal that the next event needs to be re-checked, in case it belongs to the deleted event source
    fShouldCheckNextEvent = true;
    fFireCondVar.notify_all();
}

EventTimingController::~EventTimingController()
{
    finishImpl();

    if (fEventProcessingThread && fEventProcessingThread->joinable())
    {
        fEventProcessingThread->join();
    }
};

void EventTimingController::setSpeed(float speed)
{
    std::lock_guard<std::mutex> lock(fFireMutex);
    if (fIsInitialised)
    {
        updateTotalRunTime();
        updateTotalPauseTime();
    }
    fSpeed = speed;
}

EventTimingController::EventSourceControl* EventTimingController::findNextEventSource(TimestampType &nextEventSendingTime, std::string &nextEventTopic)
{
    std::lock_guard<std::mutex> lock(fFireMutex);
    auto nextEventSource = findNextEventSourceImpl(nextEventSendingTime, nextEventTopic);

    return nextEventSource;
}

EventTimingController::EventSourceControl* EventTimingController::findNextEventSourceImpl(TimestampType &nextEventSendingTime, std::string &nextEventTopic)
{
    // Next event to send has earliest timestamp
    EventTimingController::EventSourceControl* nextEventSource = nullptr;
    TimestampType earliestEventTime;
    std::string earliestEventTopic;

    for (size_t i = 0; i < fEventSources.size(); ++i)
    {
        TimestampType sourceNextEventTime;
        std::string sourceNextEventTopic;
        bool sourceHasTime = fEventSources[i].eventSource->getNextEventInfo(sourceNextEventTime, sourceNextEventTopic);
         
        // skip event source, if it does not have any events currently
        if (!sourceHasTime)
        {
            continue;
        }

        // compare with earliest start time found so far, if any
        if (nextEventSource)
        {
            if (sourceNextEventTime < earliestEventTime)
            {
                earliestEventTime = sourceNextEventTime;
                
                nextEventSource = &fEventSources[i];
                earliestEventTopic = sourceNextEventTopic;
            }
        }
        // otherwise this is the first start time to remember
        else
        {
            earliestEventTime = sourceNextEventTime;

            nextEventSource = &fEventSources[i];
            earliestEventTopic = sourceNextEventTopic;
        }
    }

    // if a valid event source has been found, output the corresponding event time
    if (nextEventSource)
    {
        nextEventSendingTime = earliestEventTime;
        nextEventTopic = earliestEventTopic;
        
        if (!fIsInitialised)
        {
            fSimulationStartTime = earliestEventTime;
            fPreviousRunningStartTime = static_cast<TimestampType>(std::chrono::system_clock::now());
            fIsInitialised = true;
            fInitCondVar.notify_all();
        }
    }

    return nextEventSource;
}

void EventTimingController::triggerNewEventPushed(IDynamicEventSource* pushedEventSource)
{
    std::lock_guard<std::mutex> lockFire(fFireMutex);
    triggerNewEventPushedImpl(pushedEventSource);
}

void EventTimingController::triggerNewEventPushedImpl(IDynamicEventSource* pushedEventSource)
{
    TimestampType pushedEventTime;
    std::string pushedEventTopic;

    // Get timestamp of next event from input source
    pushedEventSource->getNextEventInfo(pushedEventTime, pushedEventTopic);

    // If there is no next event or the next event time is after the pushed event, we indicate to the
    // eventProcessing() thread that it should re-check the event sources.
    if (fNextEventTime == static_cast<TimestampType>(std::chrono::system_clock::from_time_t(0)) || (pushedEventTime < fNextEventTime))
    {
        fShouldCheckNextEvent = true;
        endWaitForPushEventImpl();
        fFireCondVar.notify_all();
    }
}

void EventTimingController::eventProcessing()
{
    std::unique_lock<std::mutex> lockFire(fFireMutex);
    std::string nextEventTopic;
    setThreadName("EventTiming");

    // Loop until there is no next event
    while (!allEventSourcesFinished())
    {
        EventTimingController::EventSourceControl* nextEventSource = findNextEventSourceImpl(fNextEventTime, nextEventTopic);

        // Exit if end is signalled.
        if (fEnd)
        {
            break;
        }

        // If there is no next event source but all event sources haven't been marked as finished, we poll at a reduced 
        // frequency until a new push event arrives or all event sources are marked as finished.
        if (!nextEventSource)
        {
            std::chrono::duration<unsigned int, std::micro> pollPeriod(1000);
            fFireCondVar.wait_for(lockFire, pollPeriod, [this] { return fShouldCheckNextEvent || fEnd; });
            if (fEnd)
                break;
        }
        else
        {   
            // Call the next event callback
            if (nextEventSource && fNextEventCallback)
            {
                lockFire.unlock();
                fNextEventCallback(nextEventSource->eventSourceName, nextEventTopic);
                lockFire.lock();
            }

            // Wait for the 2 pause conditions: regular-pause which will not wake until unpaused or
            // wait-pause which will publish a new push event if it is has a timestamp before the wait
            // event.
            fFireCondVar.wait(lockFire, [this]{ return (!fPaused && !fWaitForPushEvent) || fEnd; });
            if (fEnd)
            {
                break;
            }

            // if next event is in the future, wait to fire it.
            TimestampType currentSimulationTime;
            bool timeStarted = getTimeImpl(currentSimulationTime);
            MCF_ASSERT(timeStarted, "EventTimingController must be started before beginning event processing.");

            while (fNextEventTime > currentSimulationTime && !fShouldCheckNextEvent && !fEnd)
            {
                auto waitTime = std::chrono::duration_cast<std::chrono::microseconds>((fNextEventTime - currentSimulationTime) / fSpeed);
                
                // Wait until next event. Break from wait if we have already received a new push event or receive one while waiting.
                fFireCondVar.wait_for(lockFire, waitTime, [this] { return fShouldCheckNextEvent || fEnd; });
                getTimeImpl(currentSimulationTime);
            }

            if (fEnd)
            {
                break;
            }

            // If we have received a new push event, we need to recheck the next event so we continue in the loop without firing.
            if (!fShouldCheckNextEvent)
            {
                nextEventSource->eventSource->fireEvent();
            }
        }
        fShouldCheckNextEvent = false;

    }
    finishImpl();
    lockFire.unlock();

    if (fReplayEventController)
    {
        const auto playbackModifier = mcf::ReplayEventController::PlaybackModifier::FINISH;
        fReplayEventController->setPlaybackModifier(playbackModifier);
    }
}

bool EventTimingController::allEventSourcesFinished() const
{
    bool allEventSourcesFinished = true;
    for (auto const& eventSource : fEventSources)
    {
        allEventSourcesFinished &= eventSource.eventSource->isFinished();
    }

    return allEventSourcesFinished;
}

void EventTimingController::start()
{
    fEventProcessingThread = std::make_unique<std::thread>(&EventTimingController::eventProcessing, this);
}

void EventTimingController::pause()
{
    std::lock_guard<std::mutex> lock(fFireMutex);
    pauseImpl();
}

void EventTimingController::resume()
{
    std::lock_guard<std::mutex> lock(fFireMutex);
    resumeImpl();
}

void EventTimingController::beginWaitForPushEvent()
{
    std::lock_guard<std::mutex> lock(fFireMutex);
    beginWaitForPushEventImpl();
}

void EventTimingController::endWaitForPushEvent()
{
    std::lock_guard<std::mutex> lock(fFireMutex);
    endWaitForPushEventImpl();
}

void EventTimingController::pauseImpl()
{
    if(!(fPaused || fWaitForPushEvent))
    {
        fPauseStartTime = static_cast<TimestampType>(std::chrono::system_clock::now());
    }
    fPaused = true;
}

void EventTimingController::resumeImpl()
{
    if (fPaused && !fWaitForPushEvent)
    {
        resumeSimTimeImpl();
    }
    fPaused = false;
}

void EventTimingController::beginWaitForPushEventImpl()
{
    if (!(fPaused || fWaitForPushEvent))
    {
        fPauseStartTime = static_cast<TimestampType>(std::chrono::system_clock::now());
    }
    fWaitForPushEvent = true;
}

void EventTimingController::endWaitForPushEventImpl()
{
    if (fWaitForPushEvent && !fPaused)
    {
        resumeSimTimeImpl();
    }
    fWaitForPushEvent = false;
}

void EventTimingController::resumeSimTimeImpl()
{
    updateTotalPauseTime();
    fPauseStartTime = static_cast<TimestampType>(std::chrono::system_clock::from_time_t(0));
    fFireCondVar.notify_all();
}

void EventTimingController::updateTotalPauseTime()
{
    const auto pauseTimeElapsed = calcCurrentPauseTime();
    const TimestampType currentTime(std::chrono::system_clock::now());
    fPauseStartTime = currentTime;
    fPauseTimeElapsed += pauseTimeElapsed;
}

void EventTimingController::updateTotalRunTime()
{
    const auto runTimeElapsed = calcCurrentRunTime();
    const TimestampType currentTime(std::chrono::system_clock::now());
    fPreviousRunningStartTime = currentTime;
    fRunTimeElapsed += runTimeElapsed;
}

std::chrono::microseconds EventTimingController::calcCurrentPauseTime() const
{
    std::chrono::microseconds currentPauseTime(0);

    // If we currently aren't paused, then the current pause duration should be 0.
    if (fPaused || fWaitForPushEvent)
    {
        const TimestampType currentTime(std::chrono::system_clock::now());
        currentPauseTime = std::chrono::duration_cast<std::chrono::microseconds>(
            (currentTime - fPauseStartTime) * fSpeed);
    }

    return currentPauseTime;
}

std::chrono::microseconds EventTimingController::calcCurrentRunTime() const
{
    const TimestampType currentTime(std::chrono::system_clock::now());
    const std::chrono::microseconds runDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        (currentTime - fPreviousRunningStartTime) * fSpeed);

    return runDuration;
}

void EventTimingController::finish()
{
    std::lock_guard<std::mutex> lock(fFireMutex);
    finishImpl();
}

void EventTimingController::finishImpl()
{
    fEnd = true;
    fFireCondVar.notify_all();
    fFinishCondVar.notify_all();
    fInitCondVar.notify_all();
}

bool EventTimingController::isFinished() const
{
    std::unique_lock<std::mutex> lock(fFireMutex);
    return fEnd;
}

void EventTimingController::waitTillFinished() const
{
    std::unique_lock<std::mutex> lock(fFireMutex);
    fFinishCondVar.wait(lock, [this]{ return fEnd; });
}

void EventTimingController::waitTillInitialised() const
{
    std::unique_lock<std::mutex> lock(fFireMutex);
    fInitCondVar.wait(lock, [this]{ return fIsInitialised || fEnd; });
}

float EventTimingController::getPlaybackSpeed() const
{
    std::lock_guard<std::mutex> lock(fFireMutex);
    return fSpeed;
}

bool EventTimingController::getTime(TimestampType& simulationTime) const
{
    std::lock_guard<std::mutex> lock(fFireMutex);

    return getTimeImpl(simulationTime);
}

bool EventTimingController::getTimeImpl(TimestampType& simulationTime) const
{
    if (!fIsInitialised)
    {
        return false;
    }
    else
    {
        // Calculate the time spent running and paused since the last time we updated fRunTimeElapsed
        // and fPauseTimeElapsed. These values will be updated when we modify the playback speed or 
        // pause/resume playback.
        const auto currentRunTimeElapsed = calcCurrentRunTime();
        const auto currentPauseTimeElapsed = calcCurrentPauseTime();

        const auto totalRunTime = fRunTimeElapsed + currentRunTimeElapsed;
        const auto totalPauseTime = fPauseTimeElapsed + currentPauseTimeElapsed;

        // Copy simulation time so we can use static_cast in const function.
        TimestampType simulationStartTime(fSimulationStartTime);
        simulationTime = static_cast<TimestampType>(static_cast<std::chrono::system_clock::time_point>(simulationStartTime)
                                                    + totalRunTime - totalPauseTime);
        return true;
    }
}

}   // namespace mcf
