/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/ReplayEventController.h"
#include "mcf_core/EventTimingController.h"
#include "mcf_core/ValueStore.h"
#include "mcf_core/TimestampType.h"
#include "mcf_core/LoggingMacros.h"
#include "mcf_core/ErrorMacros.h"
#include <thread>
#include <chrono>
#include <cmath>
#include "json/json.h"
#include "mcf_core/util/JsonLoader.h"
#include "mcf_core/util/JsonValueExtractor.h"
#include "mcf_core/util/MergeValues.h"

namespace mcf {


ReplayEventController::ReplayEventController(mcf::ValueStore& valueStore,
                                             Params params,
                                             bool startPaused,
                                             bool requireInitialisation,
                                             std::function<void()> pipelineEndCallback)
: fTopicTriggerFlags(params.pipelineEndTriggerNames, valueStore)
, fState(UNINITIALIZED)
, fPipelineEndCallback(pipelineEndCallback)
, fIsInitialised(false)
, fRunMode(RunMode::CONTINUOUS)
, fStartPaused(startPaused)
{
    auto nextEventCallbackFunction = std::bind(&ReplayEventController::pauseAtWaitEvent,
                                               this,
                                               std::placeholders::_1,
                                               std::placeholders::_2);
    fEventTimingController = std::make_unique<EventTimingController>(
        nextEventCallbackFunction, 
        params.speedFactor, 
        this);

    setParamsNonMutexed(params);

    // if a separate initialization procedure is not requested by the caller,
    // complete initialization here
    if (!requireInitialisation)
    {
        fIsInitialised = true;
        if (fStartPaused)
        {
            fState = PAUSED;
        }
        else
        {
            fState = PLAYBACK;
        }
    }
}


ReplayEventController::ReplayEventController(mcf::ValueStore& valueStore,
                                             bool startPaused,
                                             bool requireInitialisation,
                                             std::function<void()> pipelineEndCallback)
: ReplayEventController(valueStore, ReplayEventController::Params(), startPaused,
                        requireInitialisation, pipelineEndCallback)
{}


ReplayEventController::~ReplayEventController()
{
    auto joinThread = [](std::unique_ptr<std::thread>& thread) {
        if (thread && thread->joinable())
        {
            thread->join();
        }
    };

    finishPlaybackNonMutexed();
    joinThread(fPipelineEndCheckingThread);
    joinThread(fReplayRunningThread);
}


/**
 * Loads the ReplayEventControllerParams from a .json value.
 */
void ReplayEventController::loadReplayEventControllerConfig(
        const Json::Value& config,
        mcf::ReplayEventController::Params& params,
        float speedFactor,
        bool& startPaused)
{

    mcf::util::json::JsonValueExtractor valueExtractor("ReplayEventController");
    params.runMode = static_cast<mcf::ReplayEventController::RunMode>(valueExtractor.extractConfigInt(config["RunMode"], "RunMode"));
    params.runWithoutDrops = valueExtractor.extractConfigBool(config["RunWithoutDrops"], "RunWithoutDrops");
    params.waitInputEventName = valueExtractor.extractConfigString(config["WaitInputEventName"], "WaitInputEventName");
    params.waitInputTopicName = valueExtractor.extractConfigString(config["WaitInputTopicName"], "WaitInputTopicName");
    params.stepTimeMicroSeconds = valueExtractor.extractConfigInt(config["StepTimeMicroSeconds"], "StepTimeMicroSeconds");
    startPaused = valueExtractor.extractConfigBool(config["StartPaused"], "StartPaused");

    if (speedFactor > 0)
    {
        params.speedFactor = speedFactor;
    }
    else
    {
        params.speedFactor = valueExtractor.extractConfigFloat(config["SpeedFactor"], "SpeedFactor");
    }

    // Get pipeline triggers if available, otherwise return empty vector
    std::vector<std::string> pipelineEndTriggerNames;
    Json::ArrayIndex len = config["PipelineEndTriggerNames"].size();
    params.pipelineEndTriggerNames.reserve(len);
    for(Json::ArrayIndex i = 0; i != len; ++i)
    {
        params.pipelineEndTriggerNames.emplace_back(config["PipelineEndTriggerNames"][i].asString());
    }
}


void ReplayEventController::loadReplayEventControllerConfig(
        const std::string& configDirectory,
        mcf::ReplayEventController::Params& params,
        float speedFactor,
        bool& startPaused)
{
    std::string replayConfigFile = configDirectory + "/ReplayEventController.json";
    Json::Value jsonValue = mcf::util::json::loadFile(replayConfigFile);
    loadReplayEventControllerConfig(jsonValue, params, speedFactor, startPaused);
}

void ReplayEventController::loadReplayEventControllerConfig(
        const std::vector<std::string>& configDirectories,
        mcf::ReplayEventController::Params& params,
        float speedFactor,
        bool& startPaused)
{
    // create list of all config files
    std::vector<std::string> configFilePaths;
    for (auto configFilePath: configDirectories)
    {
        // append path separator to directory, if necessary
        configFilePath += "/ReplayEventController.json";
        configFilePaths.push_back(configFilePath);
    }

    // obtain merged config
    Json::Value config = mcf::util::json::mergeFiles(configFilePaths, true);
    loadReplayEventControllerConfig(config, params, speedFactor, startPaused);
}


void ReplayEventController::pauseAtWaitEvent(const std::string& eventSourceName, const std::string& eventTopicName)
{
    std::unique_lock<std::mutex> lock(fPlaybackMutex);

    // Check if we are in a mode that requires waiting for an input event
    if (fRunWithoutDrops)
    {
        // If the next event is the wait event, pause playback.
        if (eventSourceName == fParams.waitInputEventName && eventTopicName == fParams.waitInputTopicName)
        {
            // If pipeline finished before the wait event, we don't need to wait for the pipeline to 
            // finish, so we don't clear the pipeline end flag.
            if (!fPipelineFinishedBeforeWaitEvent)
            {
                fPipelineEndTriggered = false;
            }

            setWaitingForPipelineEndNonMutexed(true);
            fPipelineFinishedBeforeWaitEvent = false;
        }
    }
}


void ReplayEventController::pipelineEndChecker()
{
    while (getState() != FINISHED)
    {
        fTopicTriggerFlags.waitForAllTopicsModified();

        std::function<void()> pipelineEndCallback;
        {
            std::lock_guard<std::mutex> lockCallback(fPipelineEndCallbackMutex);
            pipelineEndCallback = fPipelineEndCallback;
        }
        
        if (pipelineEndCallback)
        {
            pipelineEndCallback();
        }

        std::lock_guard<std::mutex> lockCallback(fPlaybackMutex);

        // If the pipeline end trigger is still set, it means that the pipeline finished before
        // we reached the next wait event.
        if (fPipelineEndTriggered)
        {
            fPipelineFinishedBeforeWaitEvent = true;
        }
        fPipelineEndTriggered = true;
        fPlaybackCondVar.notify_all();
    }
}


void ReplayEventController::run()
{
    fEventTimingController->start();

    // Thread will check if the pipeline is finished every time a pipeline end trigger topic is 
    // modified and will call the callback. A separate thread is required so that we still get the 
    // pipeline end trigger when in STEPTIME mode.
    fPipelineEndCheckingThread = std::make_unique<std::thread>(&ReplayEventController::pipelineEndChecker, this);

    // Thread will run replay control in specified mode until interrupted by user or there are no 
    // more events to replay.
    fReplayRunningThread = std::make_unique<std::thread>(&ReplayEventController::runImpl, this);
}


void ReplayEventController::runImpl()
{
    fEventTimingController->waitTillInitialised();
    while(getState() != FINISHED)
    {
        if (getState() == PAUSED)
        {
            std::unique_lock<std::mutex> lockPlayback(fPlaybackMutex);
            fPlaybackCondVar.wait(lockPlayback, [this]{ return fState != PAUSED; });
        }

        if (getState() == PLAYBACK)
        {
            if (getRunMode() == RunMode::CONTINUOUS)
            {
                std::unique_lock<std::mutex> lockPlayback(fPlaybackMutex);
                runContinuousModeNonMutexed(lockPlayback);
                fPlaybackCondVar.wait(lockPlayback, [this]{
                    return (fRunWithoutDrops && fPipelineEndTriggered) || fRunConfigChanged;
                });
            }
            else if (getRunMode() == RunMode::SINGLESTEP)
            {
                std::unique_lock<std::mutex> lockPlayback(fPlaybackMutex);
                runSingleStepModeNonMutexed(lockPlayback);
                fPlaybackCondVar.wait(lockPlayback, [this]{
                    return (fRunWithoutDrops && fPipelineEndTriggered && fExternalStepFlag) || fRunConfigChanged;
                });
            }
            else if (getRunMode() == RunMode::STEPTIME)
            {
                std::unique_lock<std::mutex> lockPlayback(fPlaybackMutex);
                runStepTimeModeNonMutexed(lockPlayback);
                fPlaybackCondVar.wait(lockPlayback, [this]{
                    return fExternalStepFlag || fRunConfigChanged;
                });
            }
        }

        else if (getState() == UNINITIALIZED)
        {
            std::this_thread::sleep_for(std::chrono::duration<unsigned int, std::micro>(10000));
        }

        if (getState() == FINISHED)
        {
            break;
        }

        std::lock_guard<std::mutex> lockPlayback(fPlaybackMutex);
        fRunConfigChanged = false;
    }
}


void ReplayEventController::runStepTimeModeNonMutexed(std::unique_lock<std::mutex>& lockPlayback)
{
    fEventTimingController->pause();

    if (fExternalStepFlag)
    {
        TimestampType currentSimulationTime;
        fEventTimingController->getTime(currentSimulationTime);
        const TimestampType endWaitSimTime(static_cast<uint64_t>(currentSimulationTime) + fParams.stepTimeMicroSeconds);

        // If we run without drops, we wait until the specified wait time has finished according to
        // the EventTimingController simulation time. 
        if (fRunWithoutDrops)
        {
            fEventTimingController->resume();
            while (endWaitSimTime > currentSimulationTime)
            {
                const auto playbackSpeed = fEventTimingController->getPlaybackSpeed();
                const auto waitRealTime = std::chrono::duration_cast<std::chrono::microseconds>(
                    (endWaitSimTime - currentSimulationTime) / playbackSpeed);

                // Wait for user specified time unless the pipeline end has just been 
                // triggered or the state or run mode has changed. 
                fPlaybackCondVar.wait_for(lockPlayback, waitRealTime, [this]{ 
                    return (fWaitingForPipelineEnd && fPipelineEndTriggered) || fRunConfigChanged; 
                });

                if (fPipelineEndTriggered)
                {
                    setWaitingForPipelineEndNonMutexed(false);
                }

                if (fRunConfigChanged)
                {
                    break;
                }

                // Recheck simulation time since we could have been waiting at the wait_for()
                fEventTimingController->getTime(currentSimulationTime);
            }
        }
        else
        {
            const auto playbackSpeed = fEventTimingController->getPlaybackSpeed();
            const auto waitRealTime = std::chrono::duration_cast<std::chrono::microseconds>(
                (endWaitSimTime - currentSimulationTime) / playbackSpeed);

            fEventTimingController->resume();
            fPlaybackCondVar.wait_for(lockPlayback, waitRealTime, [this]{ return fRunConfigChanged; });
        };

        fExternalStepFlag = false;
        if (!fRunConfigChanged)
        {
            fEventTimingController->pause();
        }
    }
}


void ReplayEventController::runContinuousModeNonMutexed(std::unique_lock<std::mutex>& lockPlayback)
{
    if (fPipelineEndTriggered)
    {
        setWaitingForPipelineEndNonMutexed(false);
    }
}


void ReplayEventController::runSingleStepModeNonMutexed(std::unique_lock<std::mutex>& lockPlayback)
{
    if (fPipelineEndTriggered && fExternalStepFlag)
    {
        setWaitingForPipelineEndNonMutexed(false);
        fExternalStepFlag = false;
    }
}


void ReplayEventController::setPlaybackModifier(const PlaybackModifier& playbackModifier)
{
    std::lock_guard<std::mutex> lock(fPlaybackMutex);
    if (playbackModifier == PlaybackModifier::PAUSE)
    {
        pausePlaybackNonMutexed();
    }
    else if (playbackModifier == PlaybackModifier::RESUME)
    {
        resumePlaybackNonMutexed();
    }
    else if (playbackModifier == PlaybackModifier::STEPONCE)
    {
        stepPlaybackOnceNonMutexed();
    }
    else if (playbackModifier == PlaybackModifier::FINISH)
    {
        finishPlaybackNonMutexed();
    }
}


void ReplayEventController::stepPlaybackOnceNonMutexed()
{
    // Manually trigger the event flag as after waiting for the new step, the trigger 
    // topics may have returned, setting the flags, but the condition variable will not 
    // be woken to check them again after they're set since the topics won't be
    // republished.
    fExternalStepFlag = true;

    fTopicTriggerFlags.manuallyTriggerEvent();
    fPlaybackCondVar.notify_all();
}


void ReplayEventController::pausePlaybackNonMutexed()
{
    if (changeStateNonMutexed(PAUSED))
    {
        fEventTimingController->pause();
    }
}


void ReplayEventController::resumePlaybackNonMutexed()
{
    if (changeStateNonMutexed(PLAYBACK))
    {
        fEventTimingController->resume();

        // Manually trigger the event flag as after pausing, the trigger topics may
        // have returned, setting the flags, but the condition variable will not be
        // woken to check them again after they're set since the topics won't be
        // republished.
        fTopicTriggerFlags.manuallyTriggerEvent();

        // Wake up the system from the pause state.
        fPlaybackCondVar.notify_all();
    }
}


void ReplayEventController::finishPlaybackNonMutexed()
{
    if (changeStateNonMutexed(FINISHED))
    {
        if (!fEventTimingController->isFinished())
        {
            fEventTimingController->finish();
        }
        fPlaybackCondVar.notify_all();
        fTopicTriggerFlags.exitWaitForAllTopicsModified();
    }
}


void ReplayEventController::setInitialisationComplete()
{
    std::lock_guard<std::mutex> lock(fPlaybackMutex);
    fIsInitialised = true;
    setParamsNonMutexed(fParams);

    if (fStartPaused)
    {
        changeStateNonMutexed(PAUSED);
    }
    else
    {
        changeStateNonMutexed(PLAYBACK);
    }
}


void ReplayEventController::setParams(ReplayEventController::Params params)
{
    std::lock_guard<std::mutex> lock(fPlaybackMutex);
    setParamsNonMutexed(std::move(params));
}


void ReplayEventController::setParamsNonMutexed(ReplayEventController::Params params)
{
    // If speed factor wasn't changed, reset params to old speed factor.
    if (!changeSpeedFactorNonMutexed(params.speedFactor))
    {
        params.speedFactor = fParams.speedFactor;
    }
    
    // If ReplayEventController is initialised, use new parameters. Otherwise, only store them.
    if (fIsInitialised)
    {
        changeRunModeNonMutexed(params.runMode);
        fTopicTriggerFlags.updateTopics(params.pipelineEndTriggerNames);
        fRunWithoutDrops = params.runWithoutDrops;

        if (!params.runWithoutDrops)
        {
            setWaitingForPipelineEndNonMutexed(false);
        }
    }

    fParams = params;
}


ReplayEventController::Params ReplayEventController::getParams() const
{
    std::lock_guard<std::mutex> lock(fPlaybackMutex);
    return fParams;
}


ReplayEventController::State ReplayEventController::getState() const
{
    std::lock_guard<std::mutex> lock(fPlaybackMutex);
    return fState;
}


ReplayEventController::RunMode ReplayEventController::getRunMode() const
{
    std::lock_guard<std::mutex> lock(fPlaybackMutex);
    return fRunMode;
}


bool ReplayEventController::changeSpeedFactorNonMutexed(const float speedFactor)
{
    if (speedFactor > 0)
    {
        fEventTimingController->setSpeed(speedFactor);
        return true;
    }
    else
    {
        MCF_WARN_NOFILELINE("Replay speed factor cannot be negative.");
        return false;
    }
}


bool ReplayEventController::changeStateNonMutexed(const State& newState)
{
    if (fState != newState)
    {
        fRunConfigChanged = true;
        fState = newState;
        fPlaybackCondVar.notify_all();

        return true;
    }
    else
    {
        return false;
    }
}


bool ReplayEventController::isFinished() const
{
    std::lock_guard<std::mutex> lock(fPlaybackMutex);
    return fState == State::FINISHED;
}


void ReplayEventController::changeRunModeNonMutexed(const RunMode& newRunMode)
{
    if (fIsInitialised)
    {
        // The steptime mode pauses playback until the user sets the external flag. Therefore, if we 
        // are exiting step time and the user has not manually paused, we resume playback.
        if (fRunMode == STEPTIME && newRunMode != STEPTIME && fState != PAUSED)
        {
            fEventTimingController->resume();
        }

        fRunConfigChanged = true;
        fRunMode = newRunMode;
        fPlaybackCondVar.notify_all();

        if (!fRunWithoutDrops)
        {
            setWaitingForPipelineEndNonMutexed(false);
        }
    }
}


void ReplayEventController::setWaitingForPipelineEndNonMutexed(bool waitingForPipelineEnd)
{
    if (waitingForPipelineEnd)
    {
        fEventTimingController->beginWaitForPushEvent();
        fWaitingForPipelineEnd = true;
    }
    else
    {
        fEventTimingController->endWaitForPushEvent();
        fWaitingForPipelineEnd = false;
    }
}


void ReplayEventController::setPipelineEndCallback(std::function<void()> pipelineEndCallback)
{
    std::lock_guard<std::mutex> lockCallback(fPipelineEndCallbackMutex);
    fPipelineEndCallback = std::move(pipelineEndCallback);
}


std::weak_ptr<IEventTimingController> ReplayEventController::getEventTimingController() const
{
    return std::weak_ptr<IEventTimingController>(fEventTimingController);
}


void ReplayEventController::addEventSource(const std::shared_ptr<ISimpleEventSource>& eventSource,
                                           const std::string& eventName)
{
    fEventTimingController->addEventSource(eventSource, eventName);
}


void ReplayEventController::addEventSource(const std::shared_ptr<IDynamicEventSource>& eventSource,
                                           const std::string& eventName)
{
    fEventTimingController->addEventSource(eventSource, eventName);
}


void ReplayEventController::removeEventSource(const std::string& eventName)
{
    fEventTimingController->removeEventSource(eventName);
}


bool ReplayEventController::getTime(TimestampType& simulationTime) const
{
    return fEventTimingController->getTime(simulationTime);
}


} // namespace mcf
