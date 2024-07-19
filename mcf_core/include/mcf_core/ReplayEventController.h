/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_REPLAYEVENTCONTROLLER_H_
#define MCF_REPLAYEVENTCONTROLLER_H_

#include "mcf_core/TopicTriggerFlags.h"
#include "json/forwards.h"

#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>


namespace mcf {

    class ISimpleEventSource;
    class IDynamicEventSource;
    class EventTimingController;
    class IEventTimingController;
    class TimestampType;

/**
 * State machine which manages playback of events depending on selected mode.
 * It provides an interface between components or other calling code and the
 * value store and EventTimingController.
 * 
 * Modes include:
 *      CONTINUOUS: Plays back events normally.
 *      SINGLESTEP: Runs until the pipeline has finished (if runWithoutDrops is set) and then waits 
 *                  an external flag to be set before continuing. Note. this will be equivalent to 
 *                  CONTINUOUS mode when runWithoutDrops is not set, as there is no pipeline finished
 *                  trigger to wait for.
 *      STEPTIME:   Allows user input to play back events for a specified time period.
 * 
 * There is also a flag that can be set or cleared which will run the playback in no drop mode:
 *      runWithoutDrops:  If true, will ensure that pipeline finishes processing without losing 
 *                        any frames. It will trigger events from all sources until a specified 
 *                        wait event and topic is due to trigger. It will then wait until a set of 
 *                        specified topics are published on, indicating that the pipeline is complete.
 * 
 * Note. Functions with the suffix "NonMutexed" require fPlaybackMutex to be locked before 
 * calling the function. These functions will not attempt to re-acquire the fPlaybackMutex.
 */
class ReplayEventController {

    enum State
    {
        UNINITIALIZED,
        PLAYBACK,
        PAUSED,
        FINISHED
    };

public:

    /**
     * See class documentation
     */
    enum RunMode
    {
        CONTINUOUS, 
        SINGLESTEP, 
        STEPTIME
    };

    /**
     * Commands to modify the playback of the ReplayEventController
     * 
     * @param PAUSE     EventTimingController will be paused which pauses simulation time and stops
     *                  events being fired. Will remain paused until the RESUME or FINISH playback
     *                  modifier is set.
     * 
     * @param RESUME    Resumes EventTimingController which resumes simulation time and begins 
     *                  firing events as normal.
     * 
     * @param STEPONCE  Runs a single step of playback when in SINGLESTEP or STEPTIME run mode.
     * 
     * @param FINISH    Finishes playback and shuts down everything.
     */
    enum PlaybackModifier
    {
        PAUSE,
        RESUME,
        STEPONCE,
        FINISH
    };

    /**
     * Parameter structure
     * 
     * @param runMode                  Current running mode
     * 
     * @param runWithoutDrops          If true, will run in no drop mode.
     * 
     * @param speedFactor              Factor for playback speed. i.e. speedFactor of 0.5 will 
     *                                 playback replay at half speed.
     * 
     * @param pipelineEndTriggerNames  Vector of names of topics which must all be received to indicate that
     *                                 a single iteration of the processing pipeline has finished.
     * 
     * @param waitInputEventName       Name of the event source for which we wait until the pipeline has finished
     *                                 processing while in NODROPS or SINGLESTEP run modes.
     *  
     * @param waitInputTopicName       Name of the event source topic for which we wait until the pipeline has finished
     *                                 processing while in NODROPS or SINGLESTEP run modes.
     * 
     * @param stepTimeMicroSeconds     Length of time period in seconds at which we step simulation time while in 
     *                                 STEPTIME mode.
     * 
     */  
    struct Params {
        RunMode runMode = CONTINUOUS;
        bool runWithoutDrops = false;
        float speedFactor = 1.0f;
        std::vector<std::string> pipelineEndTriggerNames = std::vector<std::string>();
        std::string waitInputEventName = "";
        std::string waitInputTopicName = "";
        uint64_t stepTimeMicroSeconds = 0;
    };

    /**
     * Constructor
     * 
     * @param valueStore             Reference to the value store
     * 
     * @param initialParams          Initialisation parameters
     * 
     * @param startPaused            If true, will pause playback once the ReplayEventController has 
     *                               been initialised (set with setInitialisationComplete())
     * 
     * @param requireInitialisation  If true, indicates that an initialisation procedure must be
     *                               performed by the caller. This class will stay in the UNINITIALISED
     *                               state (which restricts state changes) until initialisation is marked
     *                               as complete with setInitialisationComplete(). This function can be 
     *                               manually called or can be set in the pipelineEndCallback.
     * 
     * @param pipelineEndCallback    Callback function that will be called every time the pipeline end is 
     *                               triggered by the pipeline end trigger topics.
     */
    ReplayEventController(mcf::ValueStore& valueStore,
                          Params initialParams,
                          bool startPaused=false,
                          bool requireInitialisation=false,
                          std::function<void()> pipelineEndCallback=std::function<void()>());

    ReplayEventController(mcf::ValueStore& valueStore,
                          bool startPaused=false,
                          bool requireInitialisation=false,
                          std::function<void()> pipelineEndCallback=std::function<void()>());

    ~ReplayEventController();

    /**
     * Loads the ReplayEventControllerParams from a .json value.
     */
    static void loadReplayEventControllerConfig(const Json::Value& config,
                                                mcf::ReplayEventController::Params& params,
                                                float speedFactor,
                                                bool& startPaused);

    /**
     * Loads the ReplayEventControllerParams from a json config file.
     */
    static void loadReplayEventControllerConfig(const std::string& configDirectory,
                                                mcf::ReplayEventController::Params& params,
                                                float speedFactor,
                                                bool& startPaused);

    /**
     * Loads the ReplayEventControllerParams from a list of merged .json config files.
     */
    static void loadReplayEventControllerConfig(const std::vector<std::string>& configDirectories,
                                                mcf::ReplayEventController::Params& params,
                                                float speedFactor,
                                                bool& startPaused);

    /**
     * Runs the state machine.
     */
    void run();

    /**
     * Indicates that initialisation is complete and loads the currently saved params. This allows 
     * the run mode and playback parameters to be changed. Will begin in a paused or running state 
     * depending on the value of startPaused, set in the constructor.
     */
    void setInitialisationComplete();

    /**
     * Sets the callback function that will be called when the pipeline ends. 
     * 
     * Note 1: if the ReplayEventController is currently waiting for the pipeline to end, the old 
     * callback will be called for the current pipeline end and then the new callback will be used 
     * for subsequent pipeline ends.
     * 
     * Note 2: if the pipeline end topics have all been modified, the callback is guaranteed to be 
     * called at least once. However, if the topics are modified multiple times before the callback
     * has finished, the callback will only be called once.
     */
    void setPipelineEndCallback(std::function<void()> pipelineEndCallback);

    /**
     * Modifies the playback of the ReplayEventController. Can pause, resume, finish playback or step
     * playback once (the step command will have a different result depending on the run mode)
     */
    void setPlaybackModifier(const PlaybackModifier& playbackModifier);

    /**
     * Set the playback parameters. If the ReplayEventController has not been initialised, only the 
     * playback state (i.e. playing back, paused or finished) and the playback speed factor can be 
     * modified. The values of the other params will be saved. When initialisation is complete, the
     * saved params will be automatically loaded into the ReplayEventController.
     */
    void setParams(Params params);

    /**
     * Get the playback parameters.
     */
    ReplayEventController::Params getParams() const;

    /**
     * Get a pointer to the EventTimingController.
     */
    std::weak_ptr<IEventTimingController> getEventTimingController() const;

    /**
     * Add a simple or dynamic event source to the EventTimingController. Events from an event 
     * source will be fired when the simulation time >= their timestamp.
     */
    void addEventSource(const std::shared_ptr<ISimpleEventSource>& eventSource,
                        const std::string& eventName);
    void addEventSource(const std::shared_ptr<IDynamicEventSource>& eventSource,
                        const std::string& eventName);

    /**
     * Remove an event source from the EventTimingController.
     */
    void removeEventSource(const std::string& eventName);

    /** 
     * Check if the ReplayEventController has finished.
     * Will return true if playback has been set as finished via setPlaybackModifier() or if all of 
     * the event sources have run out of events.
     */
    bool isFinished() const;

    /**
     * Get the simulation time from the EventTimingController.
     */
    bool getTime(TimestampType& simulationTime) const;

private:

    /**
     * Replay event calling functions.
     */
    void stepPlaybackOnceNonMutexed();
    void pausePlaybackNonMutexed();
    void resumePlaybackNonMutexed();
    void finishPlaybackNonMutexed();

    void runImpl();
    void setParamsNonMutexed(Params params);
    void runContinuousModeNonMutexed(std::unique_lock<std::mutex>& lockPlayback);
    void runSingleStepModeNonMutexed(std::unique_lock<std::mutex>& lockPlayback);
    void runStepTimeModeNonMutexed(std::unique_lock<std::mutex>& lockPlayback);
    bool changeStateNonMutexed(const State& newState);
    void changeRunModeNonMutexed(const RunMode& newRunMode);
    bool changeSpeedFactorNonMutexed(float speedFactor);

    void pauseAtWaitEvent(const std::string& eventSourceName, const std::string& eventTopicName);
    void setWaitingForPipelineEndNonMutexed(bool waitingForPipelineEnd);
    void pipelineEndChecker();

    /**
     * Locks the fPlaybackMutex and returns fState.
     */
    State getState() const;
    
    /**
     * Locks the fPlaybackMutex and returns fRunMode.
     */
    RunMode getRunMode() const;

    // Playback parameters
    State fState;
    RunMode fRunMode;
    Params fParams;
    bool fRunWithoutDrops = false;

    bool fPipelineFinishedBeforeWaitEvent = false;
    bool fIsInitialised;
    const bool fStartPaused;

    std::function<void()> fPipelineEndCallback;

    TopicTriggerFlags fTopicTriggerFlags;
    std::shared_ptr<EventTimingController> fEventTimingController;
    std::unique_ptr<std::thread> fPipelineEndCheckingThread;
    std::unique_ptr<std::thread> fReplayRunningThread;
    
    std::condition_variable fPlaybackCondVar;
    mutable std::mutex fPlaybackMutex;
    mutable std::mutex fPipelineEndCallbackMutex;

    // Condvar flags
    bool fRunConfigChanged = false;         // true when fRunMode, fState or playback speed has just been changed.
    bool fPipelineEndTriggered = false;     // true when the pipeline end trigger has been received.
    bool fExternalStepFlag = false;         // true when the external flag has just been set
    bool fWaitingForPipelineEnd = false;    // true when we are currently waiting for the pipeline to end.

};

} // namespace mcf

#endif /* MCF_REPLAYEVENTCONTROLLER_H_ */
