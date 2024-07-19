/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_remote/RemoteService.h"
#include "mcf_core/ThreadName.h"

#include "mcf_core/ErrorMacros.h"

#include <chrono>

namespace mcf {

namespace remote {

namespace
{

void joinAndDelete(std::thread* t)
{
    if (t)
    {
        t->join();
        delete t;
    }
};
} // anonymous namespace;

// prefix to Component's name
static const constexpr char namePrefix[] = "RemoteService";

RemoteService::RemoteService(
    ValueStore& valueStore,
    RemotePair<ValuePtr> transceiver) :
    Component(std::string(namePrefix) +  transceiver.connectionStr(), 30),
    _valueStore(valueStore),
    _transceiver(std::move(transceiver)),
    _initialized(false)
{
}

RemoteService::~RemoteService() = default;


void RemoteService::addSendRule(
    const std::string& topic,
    const size_t queueLength,
    const bool blocking,
    const uint8_t prio)
{
    addSendRule(topic, topic, queueLength, blocking, prio);
}

void RemoteService::addSendRule(
    const std::string& topicLocal,
    const std::string& topicRemote,
    const size_t queueLength,
    const bool blocking,
    const uint8_t prio)
{
    // send only once, even if send rule is specified multiple times
    if(_sendRules.find(topicRemote) != _sendRules.end())
    {
        MCF_THROW_RUNTIME(fmt::format(
            "A send rule for remote topic '{}' has already been defined for {}",
            topicRemote,
            getName())
        );
    }

    //TODO Find a way to avoid using QueuedReceiverPort<Value> here
    //     Ideally, GenericQueuedReceiverPort (now abstract) should be used directly
    std::unique_ptr<GenericQueuedReceiverPort> port = std::make_unique<QueuedReceiverPort<Value>>(
        *this,
        fmt::format("Send[{}]", topicLocal),
        queueLength,
        blocking);

     _sendRules[topicRemote] = {
        topicLocal,
        queueLength,
        prio,
        blocking,
        SendState(),
        std::move(port)
    };
}

void RemoteService::addReceiveRule(const std::string& topic)
{
    addReceiveRule(topic, topic);
}

void RemoteService::addReceiveRule(const std::string& topicLocal, const std::string& topicRemote)
{
    // receive only once, even if receive rule is specified multiple times
    if(_receiveRules.find(topicRemote) != _receiveRules.end())
    {
        MCF_THROW_RUNTIME(fmt::format(
            "A receive rule for remote topic '{}' has already been defined for {}",
            topicRemote,
            getName())
        );
    }

    //TODO Find a way to avoid using SenderPort<Value> here
    //     Ideally, GenericSenderPort (now abstract) should be used directly
    std::unique_ptr<GenericSenderPort> port = std::make_unique<SenderPort<Value>>(
        *this,
        fmt::format("Receive[{}]", topicLocal)
    );

    _receiveRules[topicRemote] = {
        topicLocal,
        std::move(port),
        ReceiveState()
    };
}

void RemoteService::configure(IComponentConfig& config)
{
    for (auto& sendRule : _sendRules) {
        config.registerPort(*sendRule.second.port, sendRule.second.topic);
        sendRule.second.port->registerHandler([this] { handlePorts(); });
    }
    for (auto& receiveRule : _receiveRules) {
        config.registerPort(*receiveRule.second.port, receiveRule.second.topic);
    }

    registerTriggerHandler([this] { handleTriggers(); });
}

void RemoteService::startup()
{
    setThreadName("RS");

    int policy = 0;
    sched_param parameters{-1};
    pthread_getschedparam(pthread_self(), &policy, &parameters);

    _transceiver.connectSender();

    _triggerCyclicThread =
            std::unique_ptr<std::thread, std::function<void (std::thread *)>>(
                    new std::thread(&RemoteService::triggerCyclic, this),
                    joinAndDelete);

    _receivingThread =
            std::unique_ptr<std::thread, std::function<void (std::thread *)>>(
                    new std::thread(
                            &RemoteService::receive,
                            this,
                            ComponentTraceEventGenerator::getLocalInstance(),
                            policy,
                            parameters),
                    joinAndDelete);

    _pendingValuesThread =
            std::unique_ptr<std::thread, std::function<void (std::thread *)>>(
                    new std::thread(
                            &RemoteService::handlePendingValues,
                            this,
                            ComponentTraceEventGenerator::getLocalInstance(),
                            policy,
                            parameters),
                    joinAndDelete);

}

void RemoteService::shutdown()
{
    std::unique_lock<std::mutex> lock(_mtxReceive);
    _cvar_pendR.notify_all(); // wake up sleeping threads

    _transceiver.disconnectSender();
}

std::string RemoteService::valueReceived(const std::string& topic, ValuePtr value)
{
    if(!_initialized) return "REJECTED";

    std::lock_guard<std::mutex> lck(_mtxReceive);

    auto receiveRuleIt = _receiveRules.find(topic);
    if(receiveRuleIt == _receiveRules.end())
    {
        return "REJECTED";
    }

    ReceiveRule& receiveRule = receiveRuleIt->second;
    if(receiveRule.state.pendingValue != nullptr)
    {
        return "REJECTED";
    }

    auto& port = receiveRule.port;

    int inserted = port->setValue(value, false);

    if(inserted == EAGAIN)
    {
        // ValueStore: port is blocked => store value and inform pending value handler thread
        receiveRule.state.pendingValue = value;
        _cvar_pendR.notify_all();
        return "RECEIVED";
    }

    if(inserted == ENOTCONN || inserted == ECANCELED)
    {
        // Port Error: not connected or blocking write aborted
        return "REJECTED";
    }

    return "INJECTED";
}

void RemoteService::blockedValueInjectedReceived(const std::string& topic)
{
    // value has been injected on the remote side => send not pending anymore
    std::lock_guard<std::mutex> lck(_mtxSend);
    _sendRules[topic].state.sendPending = false;
    trigger();
}

void RemoteService::blockedValueRejectedReceived(const std::string& topic)
{
    // value has been rejected on the remote side => drop it locally
    std::lock_guard<std::mutex> lck(_mtxSend);
    _sendRules[topic].state.sendPending = false;
    trigger();
}

bool RemoteService::connected() const
{
    return _initialized && _transceiver.connected();
}

void RemoteService::setThreadName(const std::string& threadNamePrefix)
{
    // remove 'RemoteService' from the name, keep as much as possible from the rest, starting at the back

    const int availableCharacters = 15 - static_cast<int>(threadNamePrefix.length());
    MCF_ASSERT(availableCharacters >= 0, fmt::format(
        "threadNamePrefix {} is too long. Thread names are limited to a maximum of 15 characters",
        threadNamePrefix));

    const size_t prefixLength = (sizeof(namePrefix)/sizeof(namePrefix)[0]) - 1;
    const std::string longName = getName();

    const size_t split = static_cast<size_t>(std::max<int>(
        static_cast<int>(longName.length())-availableCharacters,
        prefixLength));
    const std::string threadName = threadNamePrefix + longName.substr(split);
    mcf::setThreadName(threadName);
}

void RemoteService::triggerCyclic()
{
    auto state = getState();
    while(state == INIT || state == STARTING_UP || state == STARTED)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        state = getState();
    }

    while(getState() == RUNNING)
    {
        trigger();

        _transceiver.waitForEvent();
    }
}

void RemoteService::receive(
    const std::shared_ptr<ComponentTraceEventGenerator>& eventGenerator,
    int policy,
    sched_param parameters)
{
    if(policy == SCHED_FIFO)
    {
        parameters.sched_priority++;
        int result = pthread_setschedparam(pthread_self(), policy, &parameters);
        if (result != 0)
        {
            MCF_ERROR_NOFILELINE(
                "Could not set scheduling parameters: policy {}, priority {}, error: {}",
                policy,
                parameters.sched_priority,
                strerror(result));
        }
        else
        {
            MCF_INFO_NOFILELINE("SET {}Receiver scheduling parameters: policy {}, priority {}, error: {}",
                getName(),
                policy,
                parameters.sched_priority,
                strerror(result));
        }
    }

    setThreadName("RR");
    ComponentTraceController::setLocalEventGenerator(eventGenerator);

    // connect receiver
    _transceiver.connectReceiver(this);

    auto state = getState();
    while(state == INIT || state == STARTING_UP || state == STARTED)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        state = getState();
    }

    while(getState() == RUNNING)
    {
        _transceiver.receive(std::chrono::milliseconds(1000));
    }

    _transceiver.disconnectReceiver();
}

void RemoteService::sendAll()
{
    std::lock_guard<std::mutex> lck(_mtxSend);
    for(auto& sendRule : _sendRules)
    {
        sendRule.second.state.forcedSend = true;
    }
    trigger();
}

void RemoteService::handlePorts()
{
    // nothing to be done, main component trigger handler will always be called
    // and handle the port event
}

void RemoteService::handleTriggers()
{
    _transceiver.observeStateChange();

    if(!_initialized && _transceiver.connected())
    {
        _initialized = true;

        auto start = std::chrono::high_resolution_clock::now();

        _transceiver.sendRequestAll();

        auto end = std::chrono::high_resolution_clock::now();
        traceDataTransferDuration(start, end, "send requestAll");
    }

    // TODO: should do this only, if initialized
    handleSend();
    handleInjectedRejected();

    _transceiver.cycle();
}

void RemoteService::handleInjectedRejected()
{
    // lock mutex to protect _insertedTopics and _rejectedTopics
    std::lock_guard<std::mutex> lock(_mtxTopics);

    // report successfully inserted values back to the sender
    for(const std::string& topic : _insertedTopics)
    {
        auto start1 = std::chrono::high_resolution_clock::now();

        std::string retVal = _transceiver.sendBlockedValueInjected(topic);

        auto end1 = std::chrono::high_resolution_clock::now();
        traceDataTransferDuration(start1, end1,
                                  fmt::format("send blockedValueInjected {}", topic));
    }

    // report rejected values back to the sender
    for(const std::string& topic : _rejectedTopics)
    {
        auto start = std::chrono::high_resolution_clock::now();

        std::string retVal = _transceiver.sendBlockedValueRejected(topic);

        auto end = std::chrono::high_resolution_clock::now();
        traceDataTransferDuration(start, end,
                                  fmt::format("send blockedValueRejected {}", topic));
    }

    _insertedTopics.clear();
    _rejectedTopics.clear();
}

void RemoteService::handleSend()
{
    if(!_initialized) return;

    bool moreValuesToSend = true;
    while(moreValuesToSend)
    {
        moreValuesToSend = false;
        std::lock_guard<std::mutex> lck(_mtxSend);
        for(auto& sendRule : /*prio_sorted*/(_sendRules))
        {
            // check remote state and send only in STATE_UP
            if(!_transceiver.connected())
            {
                break;
            }
            else
            {
                handleSendTopic(sendRule.first, sendRule.second);

                SendState& state = sendRule.second.state;
                if(state.forcedSend || sendRule.second.port->hasValue())
                {
                    moreValuesToSend = true;
                }
            }
        }
    }
}

void RemoteService::handleSendTopic(const std::string& topic, SendRule& sendRule)
{
    SendState& state = sendRule.state;
    auto& port = sendRule.port;

    // do not send anything if an old send is still pending
    if(state.sendPending) return;

    auto start = std::chrono::high_resolution_clock::now();

    if(port->hasValue())
    {
        auto value = port->peekValue();
        std::string result = _transceiver.sendValue(topic, value);

        if(result == "INJECTED" || result == "RECEIVED" || result == "REJECTED")
        {
            port->getValue();

            state.forcedSend = false;
            if(result == "RECEIVED")
            {
                state.sendPending = true;
            }
            else
            {
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        traceDataTransferDuration(start, end,
                fmt::format("send value on {}->{}: {}", sendRule.topic, topic, result));

    }
    else if(state.forcedSend)
    {
        auto value = _valueStore.getValue<Value>(sendRule.topic);
        if(!port->hasValue())
        {
            if(value->id() == 0)
            {
              state.forcedSend = false;
              return; // TODO get rid of early exits
            }

            std::string result = _transceiver.sendValue(topic, value);

            if(result == "RECEIVED" || result == "INJECTED" || result == "REJECTED")
            {
                state.forcedSend = false;
                if(result == "RECEIVED")
                {
                    state.sendPending = true;
                }
            }

            auto end = std::chrono::high_resolution_clock::now();
            traceDataTransferDuration(start, end,
                    fmt::format("forced send value on {}->{}: {}", sendRule.topic, topic, result));
        }
        else
        {
            // if a value has been written to the topic while we read a value from the ValueStore
            // we stop the forcedSend. The value from the queue will be sent in the next iteration
            state.forcedSend = false;
        }
    }

}


void RemoteService::handlePendingValues(
        const std::shared_ptr<ComponentTraceEventGenerator>& eventGenerator,
        int policy,
        sched_param parameters)
{
    // setup event tracing and policy
    setThreadName("RP");
    ComponentTraceController::setLocalEventGenerator(eventGenerator);
    if(policy == SCHED_FIFO)
    {
        parameters.sched_priority++;
        int result = pthread_setschedparam(pthread_self(), policy, &parameters);
        if (result != 0)
        {
            MCF_ERROR_NOFILELINE(
                    "Could not set scheduling parameters: policy {}, priority {}, error: {}",
                    policy,
                    parameters.sched_priority,
                    strerror(result));
        }
        else
        {
            MCF_INFO_NOFILELINE("SET {}Receiver scheduling parameters: policy {}, priority {}, error: {}",
                                getName(),
                                policy,
                                parameters.sched_priority,
                                strerror(result));
        }
    }

    // wait until end of stratup phase
    auto state = getState();
    while(state == INIT || state == STARTING_UP || state == STARTED)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        state = getState();
    }

    // loop while component is running
    while(getState() == RUNNING)
    {
        // lock mutex
        std::unique_lock<std::mutex> lock(_mtxReceive);

        // wait until there is at least one pending value, or we need to shut down
        _cvar_pendR.wait(lock, [this] { return isReceivedValuePending() || (getState() != RUNNING); });

        std::vector<std::string> insertedTopics;
        std::vector<std::string> rejectedTopics;
        for(auto& receiveRule : _receiveRules)
        {
            auto& pendingValue = receiveRule.second.state.pendingValue;
            if(pendingValue != nullptr)
            {
                int inserted = receiveRule.second.port->setValue(pendingValue, false);
                if(inserted == 0)  // value has been injected successfully
                {
                    pendingValue = nullptr;
                    insertedTopics.push_back(receiveRule.first);
                }
                else if (inserted == EAGAIN) // value not injected, retry later
                {
                    // nothing to be done here
                }
                else if (inserted == ENOTCONN || inserted == ECANCELED)  // value cannot be inserted
                {
                    pendingValue = nullptr;
                    rejectedTopics.push_back(receiveRule.first);
                }
                else // unexpected return code => handle like "rejected"
                {
                    MCF_ERROR("Unexpected return code from output port: {}", inserted);
                    rejectedTopics.push_back(receiveRule.first);
                }
            }
        }

        // unlock the mutex
        lock.unlock();

        // lock the mutex for _insertedTopics and _rejectedTopics
        std::unique_lock<std::mutex> lockTopics(_mtxTopics);
        _insertedTopics.insert(_insertedTopics.end(), insertedTopics.begin(), insertedTopics.end());
        _rejectedTopics.insert(_rejectedTopics.end(), rejectedTopics.begin(), rejectedTopics.end());
        lockTopics.unlock();

        // wake up main task to handle received and rejected topics (inform sender side)
        // Note: this has to be done by the main thread, because sockets (e.g. ZMQ) cannot
        //       easily be shared between threads.
        trigger();

        // sleep to avoid high-frequency polling when receiver ports are continuously blocked
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void RemoteService::resetPendingValues()
{
    std::lock_guard<std::mutex> lck(_mtxSend);
    for(auto& sendRule : _sendRules)
    {
        sendRule.second.state.sendPending = false;
    }
}

std::string RemoteService::getStateString()
{
    const auto remoteStateString = _transceiver.remoteStateString();

    if(_initialized)
    {
        return fmt::format("{}-{}", "RUN", remoteStateString);
    }
    else
    {
        return fmt::format("{}-{}", "INIT", remoteStateString);
    }
}

void RemoteService::traceDataTransferDuration(
    const std::chrono::high_resolution_clock::time_point& start,
    const std::chrono::high_resolution_clock::time_point& end,
    const std::string& name)
{
    auto componentTraceEventGenerator = ComponentTraceEventGenerator::getLocalInstance();
    if (componentTraceEventGenerator)
    {
        componentTraceEventGenerator->traceRemoteTransferTime(start, end, fmt::format("{} {}", name, getStateString()));
    }
}

bool RemoteService::isReceivedValuePending() const
{
    return std::any_of(_receiveRules.begin(), _receiveRules.end(),
                       [] (const auto& rule) { return (rule.second.state.pendingValue != nullptr); });
}

} // end namespace remote

} // end namespace mcf