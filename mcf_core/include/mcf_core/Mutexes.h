/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_MUTEXES_H
#define MCF_MUTEXES_H

#include <functional>
#include <pthread.h>

namespace mcf
{
/**
 * @brief A namespace for custom mutex constructs that are not found in the STL.
 *
 * This namespace contains the PosixThreadMutex class which is a thin wrapper around pthread_mutex_*
 * API functions. As per POSIX API, the mutexes can only be configured once in their lifetime, at
 * initialization time. Thus, the customization mechanism is provided by the
 * various derived classes of PosixThreadMutex.
 */
namespace mutex
{
/**
 * @brief Checks if real-time scheduling is available
 *
 * Internally, initializes a static object by briefly switching to SCHED_FIFO and back
 *
 * @return true if switching to SCHED_FIFO was successful
 * @return false otherwise
 */
bool realtimeCapabilityAvailable();

/**
 * @brief An abstract wrapper around pthread_mutex_t
 *
 * Exposes STL-compliant API (namely, the `Lockable` named requirement) and some common
 * functionality for deriving implementors.
 *
 * This class is not meant to be used directly as it explicitly does not initialize the mutex.
 * Unintended usage is disallowed by means of a protected constructor.
 */
class AbstractPosixThreadMutex
{
public:
    /// No copy constructor
    AbstractPosixThreadMutex(AbstractPosixThreadMutex&) = delete;
    /// No move constructor
    AbstractPosixThreadMutex(AbstractPosixThreadMutex&&) = delete;
    /// Common destructor
    virtual ~AbstractPosixThreadMutex();

    /**
     * @brief Acquires the mutex ownership
     *
     * If this is not possible, an exception will be thrown.
     */
    virtual void lock() = 0;
    /**
     * @brief Tries to lock the mutex without blocking
     *
     * @see AbstractPosixThreadMutex::lock()
     *
     * @return true, if the mutex has been acquired
     * @return false, if the mutex is owned by a different thread of execution
     */
    virtual bool try_lock()        = 0;
    /**
     * @brief Releases the mutex ownership
     *
     * All errors will be silently discarded
     */
    virtual void unlock() noexcept = 0;

    pthread_mutex_t* native_handle() { return &_mutex; }

protected:
    AbstractPosixThreadMutex() = default;

    int lockInternal() noexcept { return pthread_mutex_lock(&_mutex); }
    int unlockInternal() noexcept { return pthread_mutex_unlock(&_mutex); }
    int tryLockInternal() noexcept { return pthread_mutex_trylock(&_mutex); }

private:
    pthread_mutex_t _mutex{};
};

/**
 * @brief A specialization of AbstractPosixThreadMutex with simple locking and unlocking logic
 *
 * This class extracts simple (try-and-throw) locking and unlocking logic, but does not specify how
 * the mutex will be initialized.
 */
class SimpleAbstractPosixThreadMutex : public AbstractPosixThreadMutex
{
public:
    void lock() override;
    bool try_lock() override;
    void unlock() noexcept override { unlockInternal(); }

protected:
    SimpleAbstractPosixThreadMutex() = default;
};

/**
 * @brief A wrapper around a pthread_mutex_t
 *
 * This class serves as STL-compliant wrapper around a POSIX implementation of a mutex. It satisfies
 * the `Lockable` named requirement.
 *
 * This class describes a default mutex. Mutexes with specific protocols, such as priority
 * inheritance or priority ceiling, are implemented in derived classes in order to reflect different
 * runtime behaviour.
 */
class PosixThreadMutex : public SimpleAbstractPosixThreadMutex
{
public:
    /**
     * @brief Default Constructor
     *
     * Constructs a mutex with default (implementation-defined) parameters
     */
    PosixThreadMutex();
};

class PriorityInheritanceMutex : public PosixThreadMutex
{
public:
    PriorityInheritanceMutex();
};

/**
 * @brief A priority ceiling mutex wrapper
 *
 * Priority ceiling mutexes have special behaviour since they change the scheduling priority of the
 * owning thread at locking time. If the locking thread is not a realtime-scheduled thread, then
 * some libc implementations prefer to fail without acquiring the mutex. In this implementation we
 * check if the mutex has not been acquired due to this reason and actually change the scheduling
 * class to SCHED_FIFO for the mutex possession time. If something else (such as another priority
 * inheritance mutex) makes it impossible to release the scheduling policy back to SCHED_OTHER with
 * an EINVAL, no warning is issued.
 *
 * If realtime scheduling is not availiable due to missing capabilities (for example in unit tests),
 * the behaviour will default to "normal" mutex.
 *
 * Reflecting non-trivial behaviour, this is a separate class.
 */
class PriorityCeilingMutex : public AbstractPosixThreadMutex
{
public:
    PriorityCeilingMutex() = delete;
    explicit PriorityCeilingMutex(int ceiling);

    /**
     * @brief Acquires the mutex
     *
     * NOTE if the calling thread in not realtime, the scheduling mechanism will be changed for the
     * time of mutex usage to SCHED_FIFO.
     *
     * If an exception is thrown, then the mutex is not owned.
     */
    virtual void lock() override;
    /**
     * @brief Acquires the mutex in a non-blocking way
     *
     * @return true if the mutex has been acquired
     * @return false if the mutex is owned by a different thread
     */
    virtual bool try_lock() override;
    /**
     * @brief Releases the mutex
     *
     * Does not throw an exception
     */
    virtual void unlock() noexcept override;

private:
    /**
     * @brief Information related to the thread's native scheduling class
     *
     */
    struct ThreadSchedulingState
    {
        /// The scheduling policy
        int policy{};
        /// The scheduling parameters
        sched_param parameters{};
        /// Whether the state needs to be reset
        bool needReset = false;
    };
    ThreadSchedulingState _threadSchedulingState;

    static int resetScheduler(const ThreadSchedulingState threadSchedulingState)
    {
        return pthread_setschedparam(
            pthread_self(), threadSchedulingState.policy, &threadSchedulingState.parameters);
    }

    /**
     * @brief Locks the mutex with lockFunction. Changes the scheduling of the calling thread
     * to SCHED_FIFO if that scheduling mode is available
     *
     * The following postconditions are satisfied:
     * - if the mutex is unlocked, then the thread's scheduling state is kept
     * - if the mutex is locked, the previous thread's scheduling state is stored in
     * _threadSchedulingState
     * - if an exception is thrown, the mutex is not locked
     *
     * If _realtimeCapable is true and the calling thread is in SCHED_OTHER mode, the method will
     * try to switch the thread to SCHED_FIFO, priority 1, before locking the mutex and then set
     * _threadSchedulingState.needReset = true. If this does not succeed, the original scheduling
     * is restored and the error code will be returned.
     *
     * @param lockFunction The function performing the locking (for example,
     * PriorityCeilingMutex::tryLockInternal)
     * @return int The return code of the locking function
     */
    int lockWithReschedule(const std::function<int()>& lockFunction);

    bool _realtimeCapable = false;
};

} // namespace mutex
} // namespace mcf

#endif