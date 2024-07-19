/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_TRIGGER_H
#define MCF_TRIGGER_H

#include "mcf_core/ITriggerable.h"
#include "mcf_core/Mutexes.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace mcf {


/**
 *  A Trigger object is used to unblock threads waiting on it
 */
class Trigger : public ITriggerable {
public:
    Trigger() : fActive(false)
    {}

    void wait() {
        std::unique_lock<std::mutex> lk(fMutex);
        fCv.wait(lk, [this]{return fActive;});
        fActive = false;
    }

    void trigger() override {
        // TODO: add component trace event
        std::lock_guard<std::mutex> lk(fMutex);
        fActive = true;
        fCv.notify_all();
    }

private:
    std::mutex fMutex;
    std::condition_variable fCv;
    bool fActive;
};


/**
 *  A TriggerSource can be setup to notify Triggerable objects when
 *  some event happens.
 */
class TriggerSource {
public:
    TriggerSource() = default;

    void addTrigger(const std::shared_ptr<ITriggerable>& triggerable) {
        std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
        auto it = std::find_if(fTriggerables.begin(),
                               fTriggerables.end(),
                               [triggerable](const std::weak_ptr<ITriggerable>& e) { return e.lock() == triggerable; });
        if (it == fTriggerables.end()) {
            fTriggerables.push_back(triggerable);
        }
    }

    void removeTrigger(const std::shared_ptr<ITriggerable>& triggerable) {
        std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
        fTriggerables.erase(std::remove_if(fTriggerables.begin(),
                                           fTriggerables.end(),
                                           [triggerable](const std::weak_ptr<ITriggerable>& e) { return e.lock() == triggerable; }),
                            fTriggerables.end());
    }

protected:

    /**
     * Notify registered triggers
     * Note: fMutex must be locked by the caller before calling this method
     */
    void notifyTriggers() {
        bool found_expired = false;
        for (const auto& trigger : fTriggerables) {
            auto sp_trigger = trigger.lock();
            if (sp_trigger != nullptr) {
                sp_trigger->trigger();
            }
            else {
                found_expired = true;
            }
        }
        // cleanup expired weak pointers
        if (found_expired) {
            fTriggerables.erase(std::remove_if(fTriggerables.begin(),
                                               fTriggerables.end(),
                                               [](const std::weak_ptr<ITriggerable>& ptr) { return ptr.expired(); }),
                                fTriggerables.end());
        }
    }

    mutex::PriorityInheritanceMutex fMutex;

private:
    std::vector<std::weak_ptr<ITriggerable>> fTriggerables;
};

}

#endif // MCF_TRIGGER_H
