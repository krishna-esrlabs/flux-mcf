/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_TOPICTRIGGERFLAGS_H_
#define MCF_TOPICTRIGGERFLAGS_H_

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <functional>


namespace mcf {

    class ValueStore;
    class Trigger;
    class EventFlag;

/**
 * Class to manage event flags for a set of topics which are set when the 
 * value of that topic changes in the ValueStore. Contains a single trigger
 * which will be triggered when the value of any of the topics is modified.
 */
class TopicTriggerFlags
{
public:
    TopicTriggerFlags(std::vector<std::string> topicNames, mcf::ValueStore& valueStore);

    void addTopic(const std::string& topicName);
    void removeTopic(const std::string& topicName);
    void updateTopics(const std::vector<std::string>& inputTopicNames);
    void resetFlags();
    std::vector<std::string> getTopicNames() const;

    /**
     * Waits until any of the trigger topics have been modified on the value store.
     */
    void waitForAnyTopicModified() const;

    /**
     * Waits until all of the trigger topics have been modified on the value store or the 
     * exitWaitForAllTopicsModified() function has been called. Returns true if all topics have been 
     * modified and false if the exitWaitForAllTopicsModified() function has been called.
     */
    bool waitForAllTopicsModified();

    /**
     * Returns true if all of the trigger topics have been modified on the value store since the 
     * trigger flags were last cleared.
     */
    bool areAllFlagsSet() const;

    /** 
     * Lock-free implementation of areAllFlagsSet().
     */
    bool areAllFlagsSetImpl() const;
    
    /**
     * Causes waitForAllTopicsModified() to exit immediately. This does not affect the topic flags.
     */
    void exitWaitForAllTopicsModified();

    /**
     * Manually trigger class trigger which will wake up fTrigger's condition variable. This does 
     * not affect the topic flags.
     */
    void manuallyTriggerEvent();

private:
    typedef std::unordered_map<std::string, std::shared_ptr<mcf::EventFlag>>::iterator TopicFlagsIterator;

    void addTopicImpl(const std::string& topicName);
    TopicFlagsIterator removeTopicImpl(const std::string& topicName);
    void resetFlagsImpl();

    std::unordered_map<std::string, std::shared_ptr<mcf::EventFlag>> fTopicFlags;
    const std::shared_ptr<mcf::Trigger> fTrigger;

    mutable std::mutex fTopicMutex;
    mcf::ValueStore& fValueStore;

    bool fExitWaitForTopic = false;

};

} // namespace mcf

#endif /* MCF_TOPICTRIGGERFLAGS_H_ */