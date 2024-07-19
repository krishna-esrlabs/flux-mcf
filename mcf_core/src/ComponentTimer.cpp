/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/ComponentTimer.h"
#include "mcf_core/ComponentTraceEventGenerator.h"
#include "mcf_core/LoggingMacros.h"
#include "mcf_core/ComponentLogger.h"


namespace mcf {

namespace
{

using time_point = std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration>;

double timeS(struct timespec *ts)
{
    // modulo a day, to keep numbers small
    return ((double) (ts->tv_nsec)) / 1000'000'000 + (double) (ts->tv_sec % (3600 * 24));
}

time_point getRealTimeS()
{
    return std::chrono::high_resolution_clock::now();
}

double getThreadTimeS()
{
    clockid_t cid = 0;
    pthread_getcpuclockid(pthread_self(), &cid);
    struct timespec time {};
    clock_gettime(cid, &time);
    return timeS(&time);
}


/**
 * Job descriptor
 */
class Desc
{
public:

    time_point lastBeginReal;
    double lastBeginThread = 0.;

    uint64_t totRealMicros = 0UL;
    uint64_t totThreadMicros = 0UL;
    uint64_t numCalls = 0UL;
};

/**
 * Measured job duration
 */
struct JobDur
{
    uint64_t endReal = 0UL;       // job end time in microseconds since 1970
    float durationReal = 0.f;     // duration in seconds of real time
};


/**
 * Singleton keeping track of timing jobs
 */
class JobRegister
{
public:

    /**
     * Get reference to singleton instance, create instance on first call
     */
    static inline JobRegister& instance();

    static inline void begin(const std::string& loggerName,
                             const std::string& tag,
                             const mcf::LogSeverity& severity,
                             bool doLog);

    // returns real time in seconds
    static inline JobDur end(const std::string& loggerName,
                             const std::string& tag, const mcf::LogSeverity& severity,
                             bool doLog);

    /**
     * Destructor
     */
    ~JobRegister();

private:

    JobRegister() = default;

    std::mutex fJobDescMutex;
    std::map<std::string, Desc> fJobDescMap;

};

JobRegister::~JobRegister() = default;

inline JobRegister& JobRegister::instance()
{
    static JobRegister theInstance;
    return theInstance;
}

inline void JobRegister::begin(const std::string& loggerName,
                               const std::string& tag,
                               const mcf::LogSeverity& severity,
                               bool doLog)
{
    time_point now = getRealTimeS();
    time_point lastBeginReal;
    {
        std::unique_lock<std::mutex> lk(instance().fJobDescMutex);
        auto& desc = instance().fJobDescMap[loggerName + "$" + tag];

        // initialize on first call
        if (desc.lastBeginReal.time_since_epoch() == std::chrono::system_clock::duration::zero())
        {
            desc.lastBeginReal = now;
        }

        lastBeginReal = desc.lastBeginReal;
        desc.lastBeginReal = now;
        desc.lastBeginThread = getThreadTimeS();
        desc.numCalls += 1;
    }
    auto microsecs = std::chrono::duration_cast<std::chrono::microseconds>(now - lastBeginReal);
    double period = double(microsecs.count())/1000'000.;

    if (doLog)
    {
        MCF_LOG_NOFILELINE(severity, "Begin {} period: {:.6f}", tag, period);
    }
}

JobDur JobRegister::end(const std::string& loggerName,
                        const std::string& tag,
                        const mcf::LogSeverity& severity,
                        bool doLog)
{
    time_point now = getRealTimeS();
    time_point lastBeginReal;
    double lastBeginThread;
    double durationReal;
    double durationThread;
    double totalThread;
    uint64_t numCalls;
    JobDur jobDur;
    {
        std::unique_lock<std::mutex> lk(instance().fJobDescMutex);
        auto &desc = instance().fJobDescMap[loggerName + "$" + tag];
        lastBeginReal = desc.lastBeginReal;
        lastBeginThread = desc.lastBeginThread;

        auto microsecsReal = std::chrono::duration_cast<std::chrono::microseconds>(now - lastBeginReal);
        durationReal = double(microsecsReal.count()) / 1'000'000.;
        durationThread = getThreadTimeS() - lastBeginThread;
        if (durationThread < 0)
        {
            durationThread += 3600 * 24;
        }

        desc.totRealMicros += uint64_t(durationReal * 1'000'000.);
        desc.totThreadMicros += uint64_t(durationThread * 1'000'000.);
        totalThread = double(desc.totThreadMicros)/1'000'000.;
        numCalls = desc.numCalls;

        jobDur.endReal = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        jobDur.durationReal = static_cast<float>(durationReal);
    }

    if (doLog)
    {
        MCF_LOG_NOFILELINE(severity, "End {} dur_real: {:.6f}, dur_thread: {:.6f}, calls: {}, tot_thread: {:.6f}", tag,
                           durationReal, durationThread, numCalls, totalThread);
    }
    return jobDur;
}

} // anonymous namespace


ComponentTimer::ComponentTimer(std::string tag,
                               mcf::LogSeverity severity,
                               bool doBegin,
                               bool doEvent,
                               bool doLog)
: fTag(std::move(tag))
, fSeverity(severity)
, fDoEvent(doEvent)
, fDoLog(doLog)
{
    if (doBegin)
    {
        begin();
    }
}

ComponentTimer::~ComponentTimer()
{
    end();
}

void ComponentTimer::begin()
{
    if (!fIsActive)
    {
        // we need a component logger, to make a unique timer and log correctly
        if (!ComponentLogger::hasComponentLogger())
        {
            MCF_ERROR("Cannot start component timer, because component logger not set");
            return;
        }
        else
        {
            fLoggerName = ComponentLogger::getLocalLogger()->name();
        }

        JobRegister::begin(fLoggerName, fTag, fSeverity, fDoLog);
        fIsActive = true;
    }
}

void ComponentTimer::end()
{
    if (fIsActive)
    {
        auto jobDur = JobRegister::end(fLoggerName, fTag, fSeverity, fDoLog);
        fIsActive = false;

        // if trace event generator set: trace duration
        auto eventGenerator = ComponentTraceEventGenerator::getLocalInstance();
        if (eventGenerator && fDoEvent)
        {
            eventGenerator->traceExecutionTime(jobDur.endReal, jobDur.durationReal, fTag);
        }
    }
}

}
