/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/Mutexes.h"

#include "mcf_core/ErrorMacros.h"
#include "mcf_core/LoggingMacros.h"
#include "spdlog/fmt/fmt.h"

namespace mcf
{
namespace mutex
{
namespace
{
/// Mutex protecting the internal state of realtimeCapabilityAvailable()
std::mutex capabilityStateMutex;

/// A tri-state bool (like a (boost|std)::optional<bool>, but without the extra dependency)
enum OptionalBool
{
    True,
    False,
    NotInitialized
};
} // namespace

/**
 * @brief A singleton object wrapper that checks if real-time scheduling is available to us
 *
 * When called with an uninitialized internal static variable `available` (which happens only once
 * per process lifetime), this function briefly switches to a real-time scheduling class and then
 * back. On success, the function will return true from then on, false otherwise.
 *
 * @return true if it is possible to acquire real-time scheduling
 * @return false otherwise
 */
bool realtimeCapabilityAvailable()
{
    static std::atomic<OptionalBool> available(NotInitialized);

    OptionalBool isAvailable = available;
    if (isAvailable != NotInitialized)
    {
        return isAvailable == True;
    }
    std::unique_lock<std::mutex> lk(capabilityStateMutex);
    if (available == NotInitialized)
    {
        available  = False;
        auto self  = pthread_self();
        int policy = 0;
        sched_param parameters{-1};
        pthread_getschedparam(self, &policy, &parameters);

        sched_param testParameters{1};
        MCF_INFO_NOFILELINE("Switching to SCHED_FIFO to check if real-time scheduling is available...");
        if (pthread_setschedparam(self, SCHED_FIFO, &testParameters) == 0)
        {
            MCF_INFO_NOFILELINE("RT scheduling available, switching back.");
            available   = True;
            int success = pthread_setschedparam(self, policy, &parameters);
            if (success != 0)
            {
                MCF_WARN_NOFILELINE(
                    "Switching back to SCHED_OTHER did not succeed: {}", strerror(success));
            }
        }
        else
        {
            MCF_WARN_NOFILELINE("Realtime scheduling not available. Priority ceiling mutexes will "
                                "exhibit default mutex behaviour.");
        }
    }
    else
    {
        /* availability has already been checked */
    }

    return available == True;
}

AbstractPosixThreadMutex::~AbstractPosixThreadMutex()
{
    int ret = pthread_mutex_destroy(&_mutex);
    if (ret == EBUSY)
    {
        // Write an error to the console, but this situation probably will not end well in any
        // case...
        MCF_ERROR("Could not destroy mutex, it is busy");
    }
    else if (ret == EINVAL)
    {
        MCF_ERROR("Could not destroy mutex, it has been invalidated");
    }
}

void
SimpleAbstractPosixThreadMutex::lock()
{
    int ret = lockInternal();

    if (ret != 0)
    {
        MCF_THROW_RUNTIME(fmt::format("Could not lock mutex: {}", std::strerror(ret)));
    }
}

bool
SimpleAbstractPosixThreadMutex::try_lock()
{
    int ret = tryLockInternal();
    if (ret == EBUSY)
    {
        return false;
    }
    if (ret != 0)
    {
        MCF_THROW_RUNTIME(fmt::format("Could not lock mutex, error {}", ret));
    }
    return true;
}

PosixThreadMutex::PosixThreadMutex()
{
    int ret = pthread_mutex_init(native_handle(), nullptr);
    if (ret != 0)
    {
        MCF_THROW_RUNTIME(
            fmt::format("Could not initialize mutex with default attributes, error {}", ret));
    }
}

PriorityInheritanceMutex::PriorityInheritanceMutex()
{
    pthread_mutexattr_t attributes;
    int ret = pthread_mutexattr_init(&attributes);
    if (ret != 0)
    {
        MCF_THROW_RUNTIME(fmt::format("Could not initialize mutex attributes: {}", strerror(ret)));
    }
    ret = pthread_mutexattr_setprotocol(&attributes, PTHREAD_PRIO_INHERIT);
    if (ret != 0)
    {
        MCF_THROW_RUNTIME(fmt::format("Could not set mutex protocol: {}", strerror(ret)));
    }
    ret = pthread_mutex_init(native_handle(), &attributes);
    if (ret != 0)
    {
        MCF_THROW_RUNTIME(fmt::format("Could not initialize mutex: {}", strerror(ret)));
    }

    pthread_mutexattr_destroy(&attributes);
}

PriorityCeilingMutex::PriorityCeilingMutex(int ceiling)
{
    // check if RT is available
    if (realtimeCapabilityAvailable())
    {
        // check if ceiling is positive and in the correct bounds
        int min = sched_get_priority_min(SCHED_FIFO);
        int max = sched_get_priority_max(SCHED_FIFO);
        if (ceiling < min || ceiling > max)
        {
            MCF_THROW_RUNTIME("Priority ceiling out of bounds");
        }

        pthread_mutexattr_t attributes;
        int ret = pthread_mutexattr_init(&attributes);
        if (ret != 0)
        {
            MCF_THROW_RUNTIME(fmt::format("Could not initialize mutex attributes: {}", strerror(ret)));
        }
        ret = pthread_mutexattr_setprotocol(&attributes, PTHREAD_PRIO_PROTECT);
        if (ret != 0)
        {
            MCF_THROW_RUNTIME(fmt::format("Could not set mutex protocol: {}", strerror(ret)));
        }
        ret = pthread_mutexattr_setprioceiling(&attributes, ceiling);
        if (ret != 0)
        {
            MCF_THROW_RUNTIME(fmt::format("Could not set mutex ceiling: {}", strerror(ret)));
        }
        ret = pthread_mutex_init(native_handle(), &attributes);
        if (ret != 0)
        {
            MCF_THROW_RUNTIME(fmt::format("Could not initialize mutex: {}", strerror(ret)));
        }
        pthread_mutexattr_destroy(&attributes);

        _realtimeCapable = true;
    }
    else
    {
        pthread_mutex_init(native_handle(), nullptr);

        _realtimeCapable = false;
    }
}

int
PriorityCeilingMutex::lockWithReschedule(const std::function<int()>& lockFunction)
{
    auto self = pthread_self();
    ThreadSchedulingState threadSchedulingState;
    threadSchedulingState.needReset = false;

    pthread_getschedparam(
        self, &threadSchedulingState.policy, &threadSchedulingState.parameters);

    // When the current current scheduling scheme is not real-time, trying to acquire a
    // PTHREAD_PRIO_PROTECT mutex may lead to unexpected behavior
    // First, there is a very annoying feature of several libc implementations
    // https://sourceware.org/pipermail/libc-alpha/2008-March/022382.html
    // (mostly due to this: locking with PTHREAD_PRIO_PROTECT requires to call the scheduler,
    // and if this requires too many operations, the implementations default to not lock the
    // mutex at all).
    // Second, there seems to be a bug in the implemenation of Ubuntu 18.04 (and others) which
    // does change the scheduling scheme to real time and prohibits  changing it back and/or
    // lowering the priority afterwards even though the locking fails.
    // https://sourceware.org/bugzilla/show_bug.cgi?id=25943
    // Hence, we avoid those scenarios by always changing from SCHED_OTHER to SCHED_FIFO before
    // acquiring a PriorityCeilingMutex if realtime scheduling is available
    if (_realtimeCapable && threadSchedulingState.policy == SCHED_OTHER)
    {
        sched_param parameters{1};
        if (pthread_setschedparam(self, SCHED_FIFO, &parameters) != 0)
        {
            MCF_THROW_RUNTIME("Cannot set real-time scheduling policy");
        }
        threadSchedulingState.needReset = true;
    }

    int ret = lockFunction();

    if (ret != 0)
    {
        // this is an error, will be reported by calling function
        resetScheduler(threadSchedulingState);
    }
    else
    {
        // update threadSchedulingState
        _threadSchedulingState = threadSchedulingState;
    }

    return ret;
}

void
PriorityCeilingMutex::lock()
{
    auto lambda = [this]() { return lockInternal(); };
    int ret     = lockWithReschedule(lambda);

    if (ret != 0)
    {
        auto error = fmt::format("Cannot lock mutex: {}", strerror(ret));
        MCF_ERROR(error);
        MCF_THROW_RUNTIME(error);
    }
}

bool
PriorityCeilingMutex::try_lock()
{
    auto lambda = [this]() { return tryLockInternal(); };
    int ret     = lockWithReschedule(lambda);

    bool success = false;
    if (ret == EBUSY)
    {
        success = false;
    }
    else if (ret != 0)
    {
        auto error = fmt::format("Cannot lock mutex: {}", strerror(ret));
        MCF_ERROR(error);
        MCF_THROW_RUNTIME(error);
    }
    else
    {
        success = true;
    }
    return success;
}

void
PriorityCeilingMutex::unlock() noexcept
{
    // query value before unlocking the mutex
    ThreadSchedulingState threadSchedulingState = _threadSchedulingState;
    _threadSchedulingState.needReset = false;

    int ret = unlockInternal();
    // check if we own the mutex and if we have changed the thread's scheduling policy
    if (ret != EPERM && threadSchedulingState.needReset)
    {
        // reset the scheduling policy
        int error = resetScheduler(threadSchedulingState);
        if ((error != 0) && error != EINVAL)
        {
            // EINVAL can happen if the thread owns another priority ceiling mutex, so this error will be silently ignored
            MCF_WARN_NOFILELINE("Could not reset scheduling state: {}", strerror(error));
        }
    }
}

} // namespace mutex
} // namespace mcf