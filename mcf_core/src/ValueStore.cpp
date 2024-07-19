/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/ErrorMacros.h"
#include "mcf_core/LoggingMacros.h"
#include "mcf_core/PortTriggerHandler.h"
#include "mcf_core/ValueStore.h"

#include "msgpack.hpp"

#include <algorithm>
#include <cerrno>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace mcf {

namespace
{

void waitBlockedReceivers(std::vector<std::weak_ptr<IValueReceiver>> &receivers,
                          const std::string &key,
                          const std::function<bool()>& checkAbort)
{
    for (const auto &receiver : receivers)
    {
        auto sp_receiver = receiver.lock();
        if (sp_receiver != nullptr)
        {
            sp_receiver->waitBlocked(key, checkAbort);
        }
    }
}

// TODO: shouldn't ValuePtr become a const reference? => need to adapt receiver API
void notifyReceiversAndCleanup(std::vector<std::weak_ptr<IValueReceiver>>& receivers,
                               const std::string& key, ValuePtr vp) {
    bool found_expired = false;
    for (const auto& receiver : receivers) {
        auto sp_receiver = receiver.lock();
        if (sp_receiver != nullptr) {
            sp_receiver->receive(key, vp);
        }
        else {
            found_expired = true;
        }
    }
    // cleanup expired weak pointers
    if (found_expired) {
        receivers.erase(std::remove_if(receivers.begin(), receivers.end(),
                                       [](const std::weak_ptr<IValueReceiver>& ptr){return ptr.expired();}), receivers.end());
    }
}

bool isAnyReceiverBlocked(std::vector<std::weak_ptr<IValueReceiver>>& receivers,
                                      const std::string& key) {
    for (const auto& receiver : receivers) {
        auto sp_receiver = receiver.lock();
        if (sp_receiver != nullptr) {
            if (sp_receiver->isBlocked(key)) {
                return true;
            }
        }
    }
    return false;
}

} // anonymous namespace


const char* QueueEmptyException::what() const noexcept {
    return "queue empty";
}

bool ValueQueue::empty()
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    return fQueue.empty();
}

std::size_t ValueQueue::size()
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    return fQueue.size();
}

bool ValueQueue::getBlocking()
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    return fBlocking;
}

void ValueQueue::setBlocking(bool blocking) {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    fBlocking = blocking;
    fUnblockCv.notify_all();
}

size_t ValueQueue::getMaxLength()
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    return fMaxLength;
}

void ValueQueue::setMaxLength(size_t maxLength) {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    fMaxLength = maxLength;
    while (fMaxLength > 0 && fQueue.size() > fMaxLength) {
        fQueue.pop_front();
    }
    fUnblockCv.notify_all();
}

void ValueQueue::receive(const std::string& topic, ValuePtr& value)  {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    if (fMaxLength > 0 && fQueue.size() >= fMaxLength) {
        fQueue.pop_front();
    }
    fQueue.emplace_back(value, topic);
    notifyTriggers();
}

bool ValueQueue::isBlocked(const std::string& /* topic */) {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    return isBlockedInternal();
}

void ValueQueue::waitBlocked(const std::string& key, const std::function<bool()>& checkAbort) {
    auto blockCondition = [this, &checkAbort] { return isBlockedInternal() && !checkAbort(); };
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    timespec ts {};

    while(blockCondition())
    {
        // get current time
        clock_gettime(CLOCK_REALTIME, &ts);

        // add 10ms
        ts.tv_nsec += 10'000'000L;  // 10 ms
        if (ts.tv_nsec > 999'999'999UL)
        {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1'000'000'000UL;
        }

        int rc = 0;
        while (blockCondition() && rc == 0 )  // avoid recalculation of polling end time for spurious wake-ups
        {
            rc = pthread_cond_timedwait(fUnblockCv.native_handle(), fMutex.native_handle(), &ts);
        }
        if (rc != 0 && rc != ETIMEDOUT)
        {
            MCF_ERROR("Unexpected return value from pthread_cond_timedwait: {} ", rc);
            MCF_THROW_RUNTIME("Unexpected return value from pthread_cond_timedwait");
        }
    }
}


bool EventQueue::empty() {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    return fQueue.empty();
}

std::string EventQueue::pop() {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    if (!fQueue.empty()) {
        auto topic = fQueue.front();
        fQueue.pop_front();
        return topic;
    }
    else {
        throw QueueEmptyException();
    }
}

void EventQueue::receive(const std::string& topic, ValuePtr& value) {
    (void)value;
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    if (fMaxLength > 0 && fQueue.size() >= fMaxLength) {
        fQueue.pop_front();
    }
    fQueue.push_back(topic);
    notifyTriggers();
}


bool EventFlag::active() {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    return fActive;
}

void EventFlag::reset() {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    fActive = false;
}

std::string EventFlag::getTopic() {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    return fTopic;
}

void EventFlag::getLastTrigger(std::chrono::high_resolution_clock::time_point* lastTime,
                    std::string* lastTopic) {
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    getLastTriggerUnlocked(lastTime, lastTopic);
}

void
EventFlag::receive(const std::string& topic, ValuePtr& /*value*/)
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    fActive = true;
    fTopic = topic;

    // record time of event for evaluation (needed in particular for tracing)
    fTime = std::chrono::high_resolution_clock::now();

    notifyTriggers();
}

void EventFlag::getLastTriggerUnlocked(std::chrono::high_resolution_clock::time_point* lastTime,
                                       std::string* lastTopic) {
    if (lastTime != nullptr)
    {
        *lastTime = fTime;
    }
    if (lastTopic != nullptr)
    {
        *lastTopic = fTopic;
    }
}


void ValueStore::addReceiver(const std::string& key, const std::shared_ptr<IValueReceiver>& receiver) {
    std::lock_guard<mutex::PriorityCeilingMutex> lk(fMutex);
    std::lock_guard<mutex::PriorityCeilingMutex> entryLock(fMap[key].mutex);
    auto& receivers = fMap[key].receivers;
    auto it = std::find_if(receivers.begin(), receivers.end(),
                           [receiver](const std::weak_ptr<IValueReceiver>& e){ return e.lock() == receiver;});
    if (it == receivers.end()) {
        receivers.push_back(receiver);
    }
}

void ValueStore::removeReceiver(const std::string& key, const std::shared_ptr<IValueReceiver>& receiver) {
    std::lock_guard<mutex::PriorityCeilingMutex> lk(fMutex);
    std::lock_guard<mutex::PriorityCeilingMutex> entryLock(fMap[key].mutex);
    auto& receivers = fMap[key].receivers;
    receivers.erase(std::remove_if(receivers.begin(), receivers.end(),
                                   [receiver](const std::weak_ptr<IValueReceiver>& e){ return e.lock() == receiver;}), receivers.end());
}

void ValueStore::addAllTopicReceiver(const std::shared_ptr<IValueReceiver>& receiver) {
    std::lock_guard<mutex::PriorityCeilingMutex> lk(fMutex);
    auto it = std::find_if(fAllTopicReceivers.begin(), fAllTopicReceivers.end(),
                           [receiver](const std::weak_ptr<IValueReceiver>& e){ return e.lock() == receiver;});
    if (it == fAllTopicReceivers.end()) {
        fAllTopicReceivers.push_back(receiver);
    }
}

void ValueStore::removeAllTopicReceiver(const std::shared_ptr<IValueReceiver>& receiver) {
    std::lock_guard<mutex::PriorityCeilingMutex> lk(fMutex);
    fAllTopicReceivers.erase(std::remove_if(fAllTopicReceivers.begin(), fAllTopicReceivers.end(),
                                            [receiver](const std::weak_ptr<IValueReceiver>& e){ return e.lock() == receiver;}), fAllTopicReceivers.end());
}

int ValueStore::setValue(const std::string& key, const ValuePtr& vp, bool blocking,
                         const std::function<bool()>& checkAbort)
{
    const auto entryTime = std::chrono::high_resolution_clock::now();
    std::unique_lock<mutex::PriorityCeilingMutex> mapLock(fMutex);
    auto& entry = fMap[key];

    std::unique_lock<mutex::PriorityCeilingMutex> entryLock(entry.mutex);
    mapLock.unlock();

    // TODO: We probably have an issue here: 'entry' is a *reference* to an element of 'fMap' so it might
    //                                       be modified or even become invalid after unlocking 'mapLock'.

    auto& receivers = entry.receivers;

    if(!blocking)
    {
        if(isAnyReceiverBlocked(receivers, key))
        {
            return EAGAIN;
        }
    }
    else
    {
        // while there are blocked receivers and the user did not request to abort writing
        while (isAnyReceiverBlocked(receivers, key) && !checkAbort()) {
            auto receiversCopy = receivers;
            entryLock.unlock();
            waitBlockedReceivers(receiversCopy, key, checkAbort);
            entryLock.lock();
        }
    }

    // cancel, if user has requested to abort writing
    if (checkAbort())
    {
        return ECANCELED;
    }

    /*
     * Create a temporary shared_ptr<Value> that will hold the value in entry, forcing it to be
     * deallocated not earlier than at exit from this function, outside of the critical section,
     * since deallocations can be blocking. This does not get rid of blocking, but defers it to a
     * non-time-critical part of the execution.
     */
    ValuePtr temp = vp;
    entry.value.swap(temp);
    notifyReceiversAndCleanup(fAllTopicReceivers, key, vp);  // TODO: probably the map lock should be acquired before cleaning up fAllTopicReceivers
    notifyReceiversAndCleanup(receivers, key, vp);
    entryLock.unlock();
    const auto exitTime  = std::chrono::high_resolution_clock::now();
    auto generator = ComponentTraceController::getLocalEventGenerator();
    if (generator && !generator->isTracingTopic(key)) // avoid recursion
    {
        generator->traceExecutionTime(entryTime, exitTime, "valueStoreWrite");
    }

    return 0;
}


bool ValueStore::hasValue(const std::string& key) const {
    std::lock_guard<mutex::PriorityCeilingMutex> lk(fMutex);
    return fMap.find(key) != fMap.end();
}


msgpack::object ValueStore::getValueMsgpack(const std::string& key) const {
    ValuePtr value = nullptr;
    try {
        std::unique_lock<mutex::PriorityCeilingMutex> mapLock(fMutex);
        const auto& entry = fMap.at(key);
        std::unique_lock<mutex::PriorityCeilingMutex> entryLock(entry.mutex);
        mapLock.unlock();
        value = entry.value;
    }
    catch (std::out_of_range& e) {
        return msgpack::object();
    }
    auto typeinfoPtr = getTypeInfo(*value);

    // TODO: make more efficient
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);

    if (typeinfoPtr != nullptr) {
        const void* ptr = nullptr;
        size_t len{0};
        typeinfoPtr->packFunc(pk, value, ptr, len, false);
    }
    else {
        pk.pack("serialization error");
    }

    msgpack::unpacker pac;
    pac.reserve_buffer(buffer.size());
    memcpy(pac.buffer(), buffer.data(), buffer.size());
    pac.buffer_consumed(buffer.size());

    msgpack::object_handle oh;
    pac.next(oh);
    return oh.get();
}

std::vector<std::string> ValueStore::getKeys() const {
    std::vector<std::string> keys;
    std::lock_guard<mutex::PriorityCeilingMutex> lk(fMutex);
    for (auto const& e : fMap) {
        keys.push_back(e.first);
    }
    return keys;
}

}
