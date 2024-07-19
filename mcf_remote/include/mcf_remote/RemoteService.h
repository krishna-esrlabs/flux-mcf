/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_SERVICE_H
#define MCF_REMOTE_SERVICE_H

#include "mcf_core/Component.h"
#include "mcf_remote/IComEventListener.h"
#include "mcf_remote/RemotePair.h"

// RemoteSender.h and RemoteReceiver.h are only needed for backward compatibility of
// RemoteService.h and can be removed once the code using it is adapted to either use RemoteServie
// or include RemoteSender.h/RemoteReceiver.h directly
#include "mcf_remote/RemoteSender.h"
#include "mcf_remote/RemoteReceiver.h"

#include "mcf_core/Mcf.h"

#include <chrono>
#include <mutex>
#include <condition_variable>

namespace mcf {

namespace remote {

/**
 * Component to exchange mcf Values between processes.
 */
class RemoteService final: public Component, IRemoteEndpoint<ValuePtr>
{
    struct SendState
    {
        bool forcedSend = false;
        bool sendPending = false; // sent without ack
    };

    struct SendRule
    {
        std::string topic;
        size_t queueLength;
        uint8_t prio;
        bool blocking;
        SendState state;
        std::unique_ptr<GenericQueuedReceiverPort> port;
    };

    struct ReceiveState
    {
        ValuePtr pendingValue = nullptr;
    };

    struct ReceiveRule
    {
        std::string topic;
        std::unique_ptr<GenericSenderPort> port;
        ReceiveState state;
    };

public:
    /**
     * Constructor
     * @param valueStore  The value store in which the received values will be inserted
     * @param transceiver Class responsible for the low level sending and receiving operations
     */
    explicit RemoteService(ValueStore& valueStore, RemotePair<ValuePtr> transceiver);

    ~RemoteService() override;

    /**
     * Add a sending rule.
     *
     * MUST be called before ComponentManager configure() call
     *
     * @param topic           The topic whose values shall be forwarded to the target, using topic
     *                        as both local and remote topic
     * @param queueLength     Maximum number of values that are queued before forwarding
     * @param prio            The sending priority of this rule
     * @param blocking        If true the sender will be blocked blocked if the queue is full
     *                                until at least one Value has been taken from the queue
     *                        If false new Values will be dropped if the queue is full
     */
    void addSendRule(
        const std::string& topic,
        size_t queueLength=1,
        bool blocking=false,
        uint8_t prio=0);

    /**
     * Add a sending rule.
     *
     * MUST be called before ComponentManager configure() call
     *
     * @param topicLocal      The topic whose values shall be forwarded from the local value store
     *                        to the target
     * @param tropicRemote    The topic name used to forward the values from topicLocal to the
     *                        target. Only one send rule for a topicRemote shall be added.
     * @param queueLength     Maximum number of values that are queued before forwarding
     * @param prio            The sending priority of this rule
     * @param blocking        If true the sender will be blocked if the queue is full
     *                                until at least one Value has been taken from the queue
     *                        If false new Values will be dropped if the queue is full
     */
    void addSendRule(
        const std::string& topicLocal,
        const std::string& topicRemote,
        size_t queueLength=1,
        bool blocking=false,
        uint8_t prio=0);

    /**
     * Add a receiving rule.
     *
     * MUST be called before ComponentManager configure() call
     *
     * @param topic  The topic whose values shall be received from the sender
     */
    void addReceiveRule(const std::string& topic);

    /**
     * Add a receiving rule.
     *
     * MUST be called before ComponentManager configure() call
     *
     * @param topicLocal  The topic name under which the received values will be put into the local
     *                    value store
     * @param topicRemote The topic whose values shall be received from the sender. Only one
     *                    receive rule per topicRemote shall be added.
     */
    void addReceiveRule(const std::string& topicLocal, const std::string& topicRemote);

    /**
     * See base class Component
     */
    void configure(IComponentConfig& config) override;
    /**
     * See base class Component
     */
    void startup() override;
    /**
     * See base class Component
     */
    void shutdown() override;

    /**
     * See base class IComEventListener
     */
    std::string valueReceived(const std::string& topic, ValuePtr value) override;

    /*
     * See base class IRemoteEndpoint
     */
    void blockedValueInjectedReceived(const std::string& topic) override;

    /*
     * See base class IRemoteEndpoint
     */
    void blockedValueRejectedReceived(const std::string& topic) override;

    /**
     * See base class IRemoteEndpoint
     */
    void triggerSendCycle() override { trigger(); }

    /**
     * Sends one value over every added sending connection where a value is available
     * on the corresponding port.
     */
    void sendAll() override;

    /**
     * Checks if a connection to the remote side is currently established
     *
     * @return true  if _initialized is true and the remote state is STATE_UP
     *         false otherwise
     */
    bool connected() const;

private:
    /**
     * Utility function to set a name for the current thread. The name will consist of a maximum
     * of 15 characters and be constructed from the argument shortNamePrefix and the string
     * returned by Component::getName(). In an attempt to preserve as much useful information as
     * possible, it takes the passed prefix and appends as many characters as possible from the
     * Component's name, starting from the back. However, it will in any case remove the length of
     * the constexpr namePrefix from the Component's name, even if this results in a name shorter
     * than 15 characters.
     *
     * @param threadNamePrefix The prefix to be used in the thread name. It must have less than 15
     *                         characters, which is the upper limit for pthread_setname_np which
     *                         is used internally
     */
    void setThreadName(const std::string& threadNamePrefix);

    /**
     * Function runs in own thread to cyclic call the trigger function so pings will be sent even
     * if no values trigger its execution
     */
    void triggerCyclic();

    /**
     * Function runs in own thread constantly queries the _receiver to receive values
     */
    void receive(
        const std::shared_ptr<ComponentTraceEventGenerator>& eventGenerator,
        int policy,
        sched_param parameters);

    void handlePorts();
    void handleTriggers();

    /**
     * Inform remote sender about values that have been injected or rejected by the local receiver
     */
    void handleInjectedRejected();


    void handleSend();
    void handleSendTopic(const std::string& topic, SendRule& sendRule);

    /**
     * Main function of thread handling injection of values into temporarily blocked topics
     */
    void handlePendingValues(const std::shared_ptr<ComponentTraceEventGenerator>& eventGenerator,
                             int policy,
                             sched_param parameters);

    /**
     * Check if there are pending received values to be injected into the value store
     * Note: the mutex `_mtxReceive` must be locked before calling this method
     */
    bool isReceivedValuePending() const;

    void resetPendingValues() override;

    /**
     * Returns the current initialization and remote state as string
     *
     * @return A string naming the current RunState and RemoteState
     */
    std::string getStateString();

    void traceDataTransferDuration(const std::chrono::high_resolution_clock::time_point& start,
            const std::chrono::high_resolution_clock::time_point& end,
            const std::string& name);

    ValueStore& _valueStore;

    RemotePair<ValuePtr> _transceiver;

    std::map<std::string, SendRule> _sendRules;
    std::map<std::string, ReceiveRule> _receiveRules;

    /**
     * Topics of pending received values that have been successfully injected into the value store
     */
    std::vector<std::string> _insertedTopics;

    /**
     * Topics of pending received values that cannot be injected into the value store
     */
    std::vector<std::string> _rejectedTopics;


    std::mutex _mtxSend;
    std::mutex _mtxReceive;
    std::mutex _mtxTopics;
    std::atomic<bool> _initialized;

    /**
     * Condition variable for waiting on pending received values
     */
    std::condition_variable _cvar_pendR;

    std::unique_ptr<std::thread, std::function<void (std::thread *)>> _triggerCyclicThread;
    std::unique_ptr<std::thread, std::function<void (std::thread *)>> _receivingThread;
    std::unique_ptr<std::thread, std::function<void (std::thread *)>> _pendingValuesThread;

};

} // end namespace remote

} // end namespace mcf

#endif