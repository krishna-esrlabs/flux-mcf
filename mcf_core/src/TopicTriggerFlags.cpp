/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/TopicTriggerFlags.h"
#include "mcf_core/ValueStore.h"
#include <algorithm>


namespace mcf {
    TopicTriggerFlags::TopicTriggerFlags(std::vector<std::string> topicNames, mcf::ValueStore& valueStore) 
    : fValueStore(valueStore)
    , fTrigger(std::make_shared<mcf::Trigger>())
    {
        std::lock_guard<std::mutex> lock(fTopicMutex);
        for (const auto& topicName : topicNames)
        {
            addTopicImpl(topicName);
        }
    }

    void TopicTriggerFlags::addTopic(const std::string& topicName)
    {
        std::lock_guard<std::mutex> lock(fTopicMutex);
        addTopicImpl(topicName);
    }

    void TopicTriggerFlags::removeTopic(const std::string& topicName)
    {
        std::lock_guard<std::mutex> lock(fTopicMutex);
        removeTopicImpl(topicName);
    }

    void TopicTriggerFlags::addTopicImpl(const std::string& topicName)
    {
        // Returns a pair consisting of an iterator to the inserted element 
        // (or to the element that prevented the insertion) and a bool 
        // denoting whether the insertion took place.
        std::pair<TopicFlagsIterator, bool> val = fTopicFlags.insert(
            {topicName, std::make_shared<mcf::EventFlag>()});
        
        if (val.second)
        {
            val.first->second->addTrigger(fTrigger);
            fValueStore.addReceiver(topicName, val.first->second);
        }
    }
    
    TopicTriggerFlags::TopicFlagsIterator TopicTriggerFlags::removeTopicImpl(const std::string& topicName)
    {
        auto it = fTopicFlags.find(topicName);
        
        if (it != fTopicFlags.end())
        {
            fValueStore.removeReceiver(it->first, it->second);
            it = fTopicFlags.erase(it);
        }

        return it;
    }

    void TopicTriggerFlags::updateTopics(const std::vector<std::string>& inputTopicNames)
    {
        // We want to track all and only topics in topicNames. Any topics that already
        // exist should not be deleted so that we don't reset their flags.
        std::lock_guard<std::mutex> lock(fTopicMutex);

        // Remove topics that are not in the input vector
        for (auto it=fTopicFlags.begin(); it!=fTopicFlags.end();)
        {
            auto inputIt = std::find(inputTopicNames.begin(), inputTopicNames.end(), it->first);

            if (inputIt == inputTopicNames.end())
            {
                it = removeTopicImpl(it->first);
            }
            else
            {
                ++it;
            }
        }

        // Add topics from input vector. addTopic checks if the topic is already stored.
        for (const auto& inputTopicName: inputTopicNames)
        {
            addTopicImpl(inputTopicName);
        }
    }

    std::vector<std::string> TopicTriggerFlags::getTopicNames() const
    {
        std::lock_guard<std::mutex> lock(fTopicMutex);

        std::vector<std::string> topicNames;
        topicNames.reserve(fTopicFlags.size());

        for (const auto& keyValuePair : fTopicFlags)
        {
            topicNames.push_back(keyValuePair.first);
        }

        return topicNames;
    }

    bool TopicTriggerFlags::areAllFlagsSet() const
    {
        std::lock_guard<std::mutex> lock(fTopicMutex);
        return areAllFlagsSetImpl();
    }

    bool TopicTriggerFlags::areAllFlagsSetImpl() const
    {
        bool allFlagsSet = true;
        for (const auto& eventFlag : fTopicFlags)
        {
            allFlagsSet &= eventFlag.second->active();
        }
        return allFlagsSet;
    }

    void TopicTriggerFlags::resetFlags()
    {
        std::lock_guard<std::mutex> lock(fTopicMutex);
        resetFlagsImpl();
    }

    void TopicTriggerFlags::resetFlagsImpl()
    {
        for (auto& eventFlag : fTopicFlags)
        {
            eventFlag.second->reset();
        }
    }

    void TopicTriggerFlags::waitForAnyTopicModified() const
    {
        fTrigger->wait();
    }

    bool TopicTriggerFlags::waitForAllTopicsModified()
    {
        std::unique_lock<std::mutex> lock(fTopicMutex);

        while (!areAllFlagsSetImpl() && !fExitWaitForTopic)
        {
            lock.unlock();
            waitForAnyTopicModified();
            lock.lock();
        }

        if (!fExitWaitForTopic)
        {
            resetFlagsImpl();
        }

        bool exitWasCalled = fExitWaitForTopic;
        fExitWaitForTopic = false;

        return exitWasCalled;
    }

    void TopicTriggerFlags::exitWaitForAllTopicsModified()
    {
        std::lock_guard<std::mutex> lock(fTopicMutex);
        fExitWaitForTopic = true;
        manuallyTriggerEvent();
    }

    void TopicTriggerFlags::manuallyTriggerEvent()
    {
        fTrigger->trigger();
    }

} // namespace mcf
