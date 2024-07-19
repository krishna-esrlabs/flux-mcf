/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/Component.h"
#include "mcf_core/ComponentTraceEventGenerator.h"
#include "mcf_core/ComponentLogger.h"
#include "mcf_core/ComponentTimer.h"
#include "mcf_core/IComponent.h"
#include "mcf_core/Messages.h"
#include "mcf_core/Port.h"
#include "mcf_core/ValueStore.h"
#include "mcf_core/ThreadName.h"

#include "json/json.h"
#include "mcf_core/util/MergeValues.h"
#include "mcf_core/ErrorMacros.h"

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace mcf {

namespace
{

#ifdef _WIN32
    constexpr const char PATH_SEP = '\\';
#else
    constexpr const char PATH_SEP = '/';
#endif

} // anonymous namespace


/*
 * Constructor
 */
Component::Component(const std::string& name, int priority)
: fName(name),
  fConfigName(name + std::string(DEFAULT_CONFIG_NAME_SUFFIX)),
  fConfigDirs(1, ComponentManager::DEFAULT_CONFIG_DIR),
  fInstanceName(name),
  fConfigOutPortTopic(std::string(DEFAULT_CONFIG_TOPIC_PATH) + name),
  fConfigInPortTopic(std::string(DEFAULT_CONFIG_TOPIC_PATH) + name),
  fThreadSchedulingParameters(SchedulingParameters{IComponent::SchedulingPolicy::Fifo, priority}),
  fRunRequest(false),
  fStopRequest(false),
  fState(INIT),
  fTrigger(std::make_shared<Trigger>()),
  fLogMessagePort(*this, "LogMessage"),
  fLogControlPort(*this, "LogControl"),
  fConfigOutPort(*this, "ConfigOut"),
  fConfigInPort(*this, "ConfigIn"),
  fConfig(),
  fComponentLogger(fName, fLogMessagePort)
{
}

/*
 * Destructor
 */
Component::~Component() = default;

void Component::ctrlConfigure(IComponentConfig& config) {
    fLogControlPort.registerHandler([this] { logControlUpdate(); });
    fInstanceName = config.instanceName();
    fComponentLogger.setName(fInstanceName);

    config.registerPort(fLogMessagePort, "/mcf/log/"+fInstanceName+"/message");
    config.registerPort(fLogControlPort, "/mcf/log/"+fInstanceName+"/control");
    config.registerPort(fConfigOutPort, fConfigOutPortTopic);
    config.registerPort(fConfigInPort, fConfigInPortTopic);
    configure(config);
};

void Component::ctrlStart() {
    if (fState == INIT || fState == STOPPED) {
        fRunRequest = false;
        fStopRequest = false;
        fState = STARTING_UP;
        fThread      = std::thread([this] { main(); });
    }
}

void Component::ctrlRun() {
    if (fState == STARTED) {
        fRunRequest = true;
    }
}

void Component::ctrlStop() {
    if (fState != INIT && fState != STOPPED) {
        fStopRequest = true;
        fTrigger->trigger();
        fThread.join();
        fState = STOPPED;
    }
}


void Component::ctrlSetSchedulingParameters(const SchedulingParameters& parameters)
{
    // Do nothing if the policy is "Default"
    if (parameters.policy == Default)
    {
        return;
    }
    // Otherwise, validate the input
    if (parameters.policy == Other && parameters.priority != 0)
    {
        MCF_THROW_RUNTIME(fmt::format("Priority {} is not valid for SCHED_OTHER", parameters.priority));
    }
    if (parameters.policy == Fifo || parameters.policy == RoundRobin)
    {
        // check validity
        int minPriority = sched_get_priority_min(parameters.policy);
        int maxPriority = sched_get_priority_max(parameters.policy);
        if (!(minPriority <= parameters.priority && parameters.priority <= maxPriority))
        {
            MCF_THROW_RUNTIME(fmt::format(
                "Scheduling priority {} out of range {}-{} for policy {}",
                parameters.priority,
                minPriority,
                maxPriority,
                parameters.policy));
        }
    }
    fThreadSchedulingParameters = parameters;

    if (fState == STARTED || fState == RUNNING)
    {
        setSchedulingPolicy();
    }
}

std::string Component::getConfigDir() const {
    MCF_WARN("Component method 'getConfigDir()' is deprecated, "
             "use 'getConfigDirs()' or 'readConfig()' instead.");

    if (fConfigDirs.empty())
    {
        return {""};
    }

    return fConfigDirs[0];
}


/*
 * return list of config directories
 */
std::vector<std::string> Component::getConfigDirs() const
{
    return fConfigDirs;
}

/*
 * Read the config file and update the configuration data accordingly
 */
void Component::readConfig()
{
    // create list of all config files
    std::vector<std::string> configFilePaths;
    for (auto configFilePath: getConfigDirs())
    {
        // determine full config file path
        if (configFilePath.back() != PATH_SEP)
        {
            // append path separator to directory, if necessary
            configFilePath += PATH_SEP;
        }
        configFilePath += getConfigName();
        configFilePaths.push_back(configFilePath);
     }

    std::cout << "Config file paths" << std::endl;
    for (const auto& path : configFilePaths)
    {
        std::cout << path << std::endl;
    }

    // obtain merged config
    fConfig = std::make_unique<Json::Value>(mcf::util::json::mergeFiles(configFilePaths, true));

    // and write to config output port
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    const std::string jsonString = Json::writeString(builder, *fConfig);
    fConfigOutPort.setValue(msg::String(jsonString));

    MCF_INFO_NOFILELINE("Setting log levels for component {}", getName());
    fComponentLogger.setLogLevelsFromConfig(*fConfig);
}

/*
 * Return component configuration data
 */
const Json::Value& Component::getConfig()
{
    // if config already available, return it
    if (fConfig)
    {
        MCF_INFO(fName + " config already available");
        fComponentLogger.setLogLevelsFromConfig(*fConfig);
        return *fConfig;
    }

    // otherwise read config from config file
    readConfig();

    return *fConfig;
}

/*
 * Get component configuration data from config input port
 */
bool Component::getConfigFromPort(Json::Value& config)
{
    // if config port does not have data, return false
    if (!fConfigInPort.hasValue())
    {
        return false;
    }

    // otherwise read and parse data
    std::string configString = fConfigInPort.getValue()->value;

    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std:: string jsonErrors;

    bool isOk = reader->parse(configString.c_str(),
                              configString.c_str() + configString.size(),
                              &config, &jsonErrors);

    if (!isOk)
    {
        MCF_THROW_RUNTIME("Failed to parse configuration: " + jsonErrors);
    }

    fComponentLogger.setLogLevelsFromConfig(config);

    return true;
}

/*
 * Register a handler which runs everytime the component thread is triggered.
 */
void Component::registerTriggerHandler(std::function<void()> handler)
{
    HandlerMapEntry e;
    e.handler = std::move(handler);
    initStats(e.statistics);
    fTriggerHandlers.push_back(e);
}

/*
 * Trigger the components thread.
 */
void Component::trigger()
{
    fTrigger->trigger();
}

void Component::registerHandler(std::shared_ptr<PortTriggerHandler> handler)
{
    auto it = std::find_if(fPortTriggerHandlers.begin(), fPortTriggerHandlers.end(), [handler](std::weak_ptr<PortTriggerHandler> e){ return e.lock() == handler;});
    if (it == fPortTriggerHandlers.end()) {
        fPortTriggerHandlers.push_back(handler);
    }
    handler->getEventFlag()->addTrigger(fTrigger);
}

void Component::unregisterHandler(std::shared_ptr<PortTriggerHandler> handler)
{
    fPortTriggerHandlers.erase(std::remove_if(fPortTriggerHandlers.begin(), fPortTriggerHandlers.end(), [handler](std::weak_ptr<PortTriggerHandler> e){ return e.lock() == handler;}), fPortTriggerHandlers.end());
    handler->getEventFlag()->removeTrigger(fTrigger);
}

/*
 * Logging API
 */
void Component::log(LogSeverity severity, const std::string& message)
{
    fComponentLogger.log(severity, message);
}

void Component::main() {

    MCF_INFO_NOFILELINE("Component [{}]: startup", fInstanceName);
    // set scheduling class to SCHED_FIFO with default priority
    fThreadHandle = pthread_self();
    setThreadName(fmt::format("{}", fInstanceName));
    setSchedulingPolicy();
    fComponentLogger.injectLocalLogger();
    ComponentTraceEventGenerator::setLocalInstance(fComponentTraceEventGenerator); // enable use of tracing macros for this thread
    startup();

    fState = STARTED;

    while(!fStopRequest && !fRunRequest) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (fRunRequest) {

        fState = RUNNING;
        while (!fStopRequest) {
            fTrigger->wait();
            if (!fStopRequest) {
                for (auto& th : fTriggerHandlers) {
                    // call the handler
                    auto start = std::chrono::high_resolution_clock::now();
                    (th.handler)();
                    auto end = std::chrono::high_resolution_clock::now();
                    calcStats(th.statistics, "*", start);
                    traceTriggerHandlerExec(start, end, th);
                }
            }
            for (const auto& handler : fPortTriggerHandlers) {
                if (!fStopRequest && handler->getEventFlag()->active()) {
                    handler->getEventFlag()->reset();
                    auto start = std::chrono::high_resolution_clock::now();
                    handler->call();
                    auto end = std::chrono::high_resolution_clock::now();
                    tracePortTriggerHandlerExec(start, end, *handler);
                }
            }
        }
    }

    fState = SHUTTING_DOWN;
    MCF_INFO_NOFILELINE("shutting down");
    shutdown();

    fState = WAIT_STOP;
}


void Component::initStats(msg::RuntimeStatsEntry& stats) {
    stats.start = 0;
    stats.count = 0;
    stats.total = 0;
    stats.min = -1;
    stats.max = 0;
}

void Component::calcStats(msg::RuntimeStatsEntry& stats, const std::string& topic, std::chrono::high_resolution_clock::time_point start) {
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
    auto msecs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    if (stats.start == 0) {
        stats.start = msecs;
    }
    stats.count += 1;
    stats.total += dur;
    if (stats.min < 0 || dur < stats.min) {
        stats.min = dur;
    }
    if (dur > stats.max) {
        stats.max = dur;
    }
    if (msecs > stats.start) {
        stats.frq = stats.count*1000/(msecs-stats.start);
    }
    stats.avg = stats.total/stats.count;
    fRuntimeStats.entries[topic] = stats;
}

void Component::traceTriggerHandlerExec(const std::chrono::high_resolution_clock::time_point& start,
                                        const std::chrono::high_resolution_clock::time_point& end,
                                        const HandlerMapEntry& triggerHandler)
{
    if (fComponentTraceEventGenerator)
    {
        // TODO: use different component trace event type
        // TODO: add some ID of handler for 'name'
        fComponentTraceEventGenerator->traceExecutionTime(start, end,
                                                          "triggerHandlers");
    }
}

void Component::tracePortTriggerHandlerExec(
        const std::chrono::high_resolution_clock::time_point& start,
        const std::chrono::high_resolution_clock::time_point& end,
        const PortTriggerHandler& handler)
{
    if (fComponentTraceEventGenerator)
    {
        fComponentTraceEventGenerator->tracePortTriggerExec(start, end, handler);
    }
}

void Component::logControlUpdate() {
    auto val = fLogControlPort.getValue();
    // This only changes the local logging level, not the global one.
    fComponentLogger.setValueStoreLogLevel(val->level);
}

void Component::setSchedulingPolicy()
{
    auto parameters = fThreadSchedulingParameters.load();
    if (parameters.policy != SCHED_OTHER && !mutex::realtimeCapabilityAvailable())
    {
        // do nothing
    }
    else
    {
        sched_param p{parameters.priority};
        int result = pthread_setschedparam(fThreadHandle, parameters.policy, &p);
        if (result != 0)
        {
            MCF_ERROR_NOFILELINE(
                "Could not set scheduling parameters: policy {}, priority {}, error: {}",
                parameters.policy,
                parameters.priority,
                strerror(result));
        }
    }
}

void Component::ctrlSetComponentTraceEventGenerator(
        const std::shared_ptr<ComponentTraceEventGenerator>& eventGenerator)
{
    fComponentTraceEventGenerator = eventGenerator;
}


}

