/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_VALUE_STORE_H
#define MCF_VALUE_STORE_H

#include "mcf_core/IExtMemValue.h"
#include "mcf_core/Value.h"
#include "mcf_core/TypeRegistry.h"
#include "mcf_core/ValueFactory.h"

#include "msgpack.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include "mcf_core/ComponentTraceController.h"
#include "mcf_core/ComponentTraceEventGenerator.h"
#include "mcf_core/Messages.h"
#include "mcf_core/Trigger.h"

namespace mcf {

template<typename T>
using ValueTopicTuple = std::tuple<std::shared_ptr<const T>, const std::string>;

class ComponentBase;
class PortTriggerHandler;

class QueueEmptyException : public std::exception {
public:
    const char* what() const noexcept override;
};

/**
 *  An IValueReceiver is a objects that can be registered
 *  with the ValueStore to be called when a value update is made.
 */
class IValueReceiver {
public:
    virtual void receive(const std::string& topic, ValuePtr& value) = 0;

    virtual bool isBlocked(const std::string& topic) { return false; }

    /**
     * Wait until the given topic can be written to (is unblocked)
     *
     * Note: in order to abort waiting, the function 'checkAbort()' must return true.
     *
     * @param topic         the topic
     * @param checkAbort    waiting is aborted when this function returns true
     */
    virtual void waitBlocked(const std::string& topic,
                             const std::function<bool()>& checkAbort) {};
};

/**
 *  A Value Queue can be registered with the ValueStore so that
 *  value updates for a specific topic are pushed into the queue.
 *
 *  The queue holds both, the actual value and the topic it came from.
 *
 *  Can act as a TriggerSource.
 */
class ValueQueue : public TriggerSource, public IValueReceiver {

public:
    explicit ValueQueue(int maxLength=0, bool blocking=false)
        : fMaxLength(maxLength),
          fBlocking(blocking)
    {}

    bool empty();

    size_t size();

    bool getBlocking();

    void setBlocking(bool blocking);

    size_t getMaxLength();

    void setMaxLength(size_t maxLength);

    template<typename T>
    std::shared_ptr<const T> peek();

    template<typename T>
    std::shared_ptr<const T> pop();

    template<typename T>
    ValueTopicTuple<T> popWithTopic();

protected:

    void receive(const std::string& topic, ValuePtr& value) override;
    bool isBlocked(const std::string& topic) override;
    void waitBlocked(const std::string& topic, const std::function<bool()>& checkAbort) override;

private:
    bool isBlockedInternal() {
        return fBlocking && fMaxLength > 0 && fQueue.size() >= fMaxLength;
    }

    typedef std::tuple<ValuePtr, const std::string> QueueEntry;

    size_t fMaxLength;
    bool fBlocking;
    std::deque<QueueEntry> fQueue;
    std::condition_variable fUnblockCv;
};


/**
 *  The EventQueue is a smaller version of the ValueQueue.
 *  It only stores the occurrence of a value update and the originating topic.
 */
class EventQueue : public TriggerSource, public IValueReceiver {
public:
    friend ComponentBase;

    explicit EventQueue(int maxLength=0)
        : fMaxLength(maxLength)
    {}

    bool empty();

    std::string pop();

protected:
    void receive(const std::string& topic, ValuePtr& value) override;

private:
    size_t fMaxLength;
    std::deque<std::string> fQueue;
};


/**
 * An EventFlag is somewhat like an EventQueue of size 1
 */
class EventFlag : public TriggerSource, public IValueReceiver {

public:
    EventFlag()
        : fActive(false)
   {}

    bool active();

    void reset();

    /**
     * Get topic of current (if active) or previous (if not active) trigger activation
     */
    std::string getTopic();

    /**
     * Determine last trigger event
     */
    void getLastTrigger(
        std::chrono::high_resolution_clock::time_point* lastTime, std::string* lastTopic);

protected:
    void receive(const std::string& topic, ValuePtr&) override;

private:

    /**
     * For now we allow PortTriggerHandler to directly call the private method getTopicUnlocked().
     * This is useful for component event tracing with as little overhead as necessary.
     *
     * Note: The currently the method trigger() of PortTriggerHandeler::TriggerTracer is called
     *       by receive() -> notifyTriggers() while fMutex is locked. Thus trigger() cannot
     *       call the public method getLastTrigger(), as it would create a recursive locking
     *       situation. On the other hand, we want to avoid turning fMutex into a recursive mutex
     *       as that would most likely increase latencies.
     *
     * TODO: Clean this up and try to minimize required mutex locks.
     */
    friend class PortTriggerHandler;

    /**
     * Determine last trigger event
     */
    void getLastTriggerUnlocked(std::chrono::high_resolution_clock::time_point* lastTime,
                                std::string* lastTopic);

    std::string fTopic;
    bool fActive;
    std::chrono::high_resolution_clock::time_point fTime;
};


/**
 * Value Store
 */
class ValueStore : public TypeRegistry {

public:
    constexpr static int VALUE_STORE_PRIORITY = 32;

    ValueStore() : fMutex(VALUE_STORE_PRIORITY) {
        mcf::msg::registerValueTypes(*this);
    }

    struct MapEntry {
        MapEntry() : mutex(VALUE_STORE_PRIORITY) {}

        MapEntry(const MapEntry&) = delete; // disallow copy constructors
        MapEntry& operator=(const MapEntry&) = delete; // also disallow copy assignment

        ValuePtr value;
        std::vector<std::weak_ptr<IValueReceiver>> receivers;
        mutable mutex::PriorityCeilingMutex mutex;
    };

    void addReceiver(const std::string& key, const std::shared_ptr<IValueReceiver>& receiver);
    void removeReceiver(const std::string& key, const std::shared_ptr<IValueReceiver>& receiver);
    void addAllTopicReceiver(const std::shared_ptr<IValueReceiver>& receiver);
    void removeAllTopicReceiver(const std::shared_ptr<IValueReceiver>& receiver);

    template<typename T, typename = std::enable_if_t<std::is_base_of<Value, T>::value>>
    int setValue(const std::string& key,
                 T&& value,
                 bool blocking=true,
                 const std::function<bool()>& checkAbort = [](){ return false; });

    template<typename T, typename = std::enable_if_t<std::is_base_of<Value, T>::value>>
    int setValue(const std::string& key,
                 const T& value,
                 bool blocking=true,
                 const std::function<bool()>& checkAbort = [](){ return false; });

    /**
     * Write a ValuePtr to the value store at the specified topic
     *
     * Note: in order to abort waiting, the function 'checkAbort()' must return true. The abort
     *       will occur at the end of the current ~10ms polling interval.
     *
     * @param key      The name of the topic
     * @param vp       A shared_ptr to the value to be written to the value store
     * @param blocking A flag indicating what should happen if any of the receivers of the topic
     *                 key is blocked when this function is called
     *                 true: The function call blocks until all receivers are not blocked any
     *                       more and writes vp to the value store.
     *                 false: The value is written to the value store if no receiver is blocked.
     *                        If one or more receivers are blocked, the function returns
     *                        immediately and the value is not written to the value store
     * @param checkAbort    waiting is aborted when this function returns true
     * @return An integer indicating either success or an error code:
     *         0:         Success, vp has been written to the value store
     *         EAGAIN:    vp was not written to the value store because one or more receivers were
     *                    blocked and the argument blocking set to false
     *         ECANCELED: vp was not written to the value store because checkAbort() returned
     *                    true before writing. May happen independently of whether 'blocking'
     *                    is set to true or false.
     */
    int setValue(const std::string& key,
                 const ValuePtr& vp,
                 bool blocking=true,
                 const std::function<bool()>& checkAbort = [](){ return false; });

    bool hasValue(const std::string& key) const;

    template<typename T>
    std::shared_ptr<const T> getValue(const std::string& key) const;

    msgpack::object getValueMsgpack(const std::string& key) const ;
    std::vector<std::string> getKeys() const;

private:

    ValueFactory fValueFactory;
    std::unordered_map<std::string, MapEntry> fMap;
    std::vector<std::weak_ptr<IValueReceiver>> fAllTopicReceivers;
    mutable mutex::PriorityCeilingMutex fMutex;
};



template<typename T>
std::shared_ptr<const T> ValueQueue::peek() {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    if (!fQueue.empty()) {
        ValuePtr ptr = std::get<0>(fQueue.front());
        return std::dynamic_pointer_cast<const T>(ptr);
    }
    else {
        throw QueueEmptyException();
    }
}

template<typename T>
std::shared_ptr<const T> ValueQueue::pop() {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    if (!fQueue.empty()) {
        ValuePtr ptr = std::get<0>(fQueue.front());
        fQueue.pop_front();
        fUnblockCv.notify_all();
        return std::dynamic_pointer_cast<const T>(ptr);
    }
    else {
        throw QueueEmptyException();
    }
}

template<typename T>
ValueTopicTuple<T> ValueQueue::popWithTopic() {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    if (!fQueue.empty()) {
        auto e = fQueue.front();
        fQueue.pop_front();
        fUnblockCv.notify_all();
        return ValueTopicTuple<T>(std::dynamic_pointer_cast<const T>(std::get<0>(e)), std::get<1>(e));
    }
    else {
        throw QueueEmptyException();
    }
}

template<typename T, typename>
inline int ValueStore::setValue(const std::string& key,
                                T&& value,
                                bool blocking,
                                const std::function<bool()>& checkAbort)
{
    ValuePtr vp
        = std::static_pointer_cast<const Value>(fValueFactory.createValue(std::forward<T>(value)));
    return setValue(key, vp, blocking, checkAbort);
}

template<typename T, typename>
inline int ValueStore::setValue(const std::string& key,
                                const T& value,
                                bool blocking,
                                const std::function<bool()>& checkAbort)
{
    static_assert(std::is_copy_constructible<T>::value, "Cannot set value by lvalue reference, because value type is not copyable");
    ValuePtr vp = std::static_pointer_cast<const Value>(fValueFactory.createValue(value));
    return setValue(key, vp, blocking, checkAbort);
}

template<typename T>
inline std::shared_ptr<const T> ValueStore::getValue(const std::string& key) const {
    const auto entryTime = std::chrono::high_resolution_clock::now();
    std::unique_lock<mutex::PriorityCeilingMutex> lk(fMutex);
    auto entry = fMap.find(key);

    if (entry != fMap.end())
    {
        std::unique_lock<mutex::PriorityCeilingMutex> topicLock(entry->second.mutex);
        lk.unlock();
        auto val = std::dynamic_pointer_cast<const T>(entry->second.value);
        topicLock.unlock();
        const auto exitTime = std::chrono::high_resolution_clock::now();
        auto generator = ComponentTraceController::getLocalEventGenerator();
        if (generator && !generator->isTracingTopic(key)) // do not trace tracing events
        {
            generator->traceExecutionTime(entryTime, exitTime, "valueStoreRead");
        }
        if (val != nullptr) {
            return val;
        }
        else {
            // map entry contains empty shared ptr
        }
    }
    return std::make_shared<const T>();
}


}

#endif
