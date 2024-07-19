/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_REMOTE_REMOTEPAIR_H
#define MCF_REMOTE_REMOTEPAIR_H

#include "mcf_remote/AbstractReceiver.h"
#include "mcf_remote/AbstractSender.h"
#include "mcf_remote/IComEventListener.h"
#include "mcf_remote/RemoteStatusTracker.h"

#include <memory>

namespace mcf
{
namespace remote
{
/**
 * @brief An interface template for remote endpoints
 *
 * @tparam ValuePtrType Type of the value the wire format deserializes to.
 */
template <typename ValuePtrType>
class IRemoteEndpoint
{
public:
    /**
     * @brief Value reception handler
     *
     * @param topic The topic of the received value
     * @param value The actual received value
     * @return A stringly-typed response
     */
    virtual std::string valueReceived(const std::string& topic, ValuePtrType value) = 0;

    /**
     * @brief Handler for the request for all available values.
     *
     */
    virtual void sendAll() = 0;

    /**
     * @brief Handler that resets pending values.
     *
     */
    virtual void resetPendingValues() = 0;

    /**
     * @brief Handler that wakes up the sending cycle.
     *
     */
    virtual void triggerSendCycle() = 0;

    /**
     * @brief Handler for the event when a blocked value on the other endpoint has been injected in
     * the remote value store.
     *
     * @param topic Topic of the value
     */
    virtual void blockedValueInjectedReceived(const std::string& topic) = 0;

    /**
     * @brief Handler for the event when a blocked value on the other endpoint has been rejected by
     * the remote value store.
     *
     * @param topic Topic of the value
     */
    virtual void blockedValueRejectedReceived(const std::string& topic) = 0;
};

/**
 * @brief A class that encapsulates a sender/receiver pair and handles timeout logic
 *
 * @tparam ValuePtrType Type of the value the wire format deserializes to.
 */
template <typename ValuePtrType>
class RemotePair : public IComEventListener<ValuePtrType>
{
public:
    /**
     * @brief Default constructor
     *
     * @param sender Sender part of the remote pair
     * @param receiver Receiver part of the remote pair
     * @param artificialJitter Maximum time of Jitter that is artificially added to simulate a slower
     *                         connection
     */
    explicit RemotePair(
        std::unique_ptr<AbstractSender> sender,
        std::unique_ptr<AbstractReceiver<ValuePtrType> > receiver,
        std::chrono::milliseconds artificialJitter = std::chrono::milliseconds(0));

    /**
     * @brief Move constructor
     *
     * This is needed to transfer the remote state tracking state
     *
     * @param other The object that is being moved from
     */
    RemotePair(RemotePair&& other)
    : _sender(std::move(other._sender))
    , _receiver(std::move(other._receiver))
    , _artificialJitter(std::move(other._artificialJitter))
    , _remoteStatusTracker(
        std::move(other._remoteStatusTracker),
        [this](uint64_t fv) -> void { _sender->sendPing(fv); })
    {
        _receiver->setEventListener(this);
    }

    virtual ~RemotePair();

    /**
     * @brief Compute a human-readable connection string
     *
     * @return A connection description string.
     */
    std::string connectionStr() const { return _sender->connectionStr(); }

    /**
     * @brief Sends a value over the wire
     *
     * @param topic The topic to send on
     * @param value The value to send
     * @return A (stringly-typed) result of the sending operation. Can be one of "INJECTED",
     * "REJECTED", "TIMEOUT"
     */
    std::string sendValue(const std::string& topic, ValuePtr value);

    /**
     * @brief Communicates to the remote point that a previously blocked value has been injected.
     *
     * @param topic The topic of the injected value
     * @return Result of the sending operation
     */
    std::string sendBlockedValueInjected(const std::string& topic);

    /**
     * @brief Communicates to the remote point that a previously blocked value has been rejected.
     *
     * @param topic The topic of the rejected value
     * @return Result of the sending operation
     */
    std::string sendBlockedValueRejected(const std::string& topic);

    /*
     * See base class IComEventListener
     */
    std::string valueReceived(const std::string& topic, ValuePtrType value) override
    {
        return _endpoint->valueReceived(topic, value);
    }

    /*
     * See base class IComEventListener
     */
    void pingReceived(uint64_t freshnessValue) override;

    /*
     * See base class IComEventListener
     */
    void pongReceived(uint64_t freshnessValue) override;

    /*
     * See base class IComEventListener
     */
    void requestAllReceived() override;

    /*
     * See base class IComEventListener
     */
    void blockedValueInjectedReceived(const std::string& topic) override
    {
        _endpoint->blockedValueInjectedReceived(topic);
    }

    /*
     * See base class IComEventListener
     */
    void blockedValueRejectedReceived(const std::string& topic) override
    {
        _endpoint->blockedValueRejectedReceived(topic);
    }

    /**
     * Checks if a connection to the remote side is currently established
     *
     * @return true  if _initialized is true and the remote state is STATE_UP
     *         false otherwise
     */
    bool connected() const;

    /**
     * Sends a control message requesing one value (if any are available) on every sendRule from
     * the receiver of this message.
     */
    void sendRequestAll() { _sender->sendRequestAll(); }

    /**
     * Returns the remote state, i.e. the current assumption if the other side of this remote pair
     * (i.e. the process we want to communicate with) is up/down or the state is not known.
     *
     * @return The assumed status of the remote side
     */
    RemoteStatusTracker::RemoteState remoteState() const { return _remoteStatusTracker.getState(); }

    /**
     * Sets the internal pointer to _endpoint and connects the _receiver
     *
     * @param endpoint Pointer to a remote endpoint to be used in future communications The pointer
     *                 is stored in this class, but the ownership/memory management responsability
     *                 stays with the calling entity.
     */
    void connectReceiver(IRemoteEndpoint<ValuePtrType>* endpoint);

    /**
     * Connects the _sender
     */
    void connectSender();

    /**
     * Resets the internal pointer to _endpoint and disconnects the _receiver.
     */
    void disconnectReceiver();

    /**
     * Disconnects the _sender.
     */
    void disconnectSender();

    /**
     * Try to receive a value. The function will return either after it received a value or when
     * the timeout has expired.
     *
     * @param timeout the number of milliseconds the function will wait for a message
     *
     * @return true if a message has been received, false if the function timed out
     */
    bool receive(std::chrono::milliseconds timeout);

    /**
     * Queries the current interval at which ping messages are sent out.
     *
     * @return The ping interval.
     */
    std::chrono::milliseconds pingInterval() { return _remoteStatusTracker.pingInterval(); }


    /**
     * Blocks the calling thread until an event is triggered in the RemoteStatusTracker.
     * This event is either as change of the remote state or the expiration of _pingInterval
     */
    void waitForEvent(){ _remoteStatusTracker.waitForEvent(); }

    /**
     * Returns a string that represents the currently assumed remote state.
     *
     * @return One of the strings "DOWN", "UNSURE", "UP"
     */
    std::string remoteStateString() const;

    /**
     * Checks if the _remoteState has changed since the last time this function was called.
     */
    void observeStateChange();

    /**
     * Triggers the runCyclic method of _remoteStatusTracker and the sending of all pongs in
     * _pongQueue
     */
    void cycle();

private:
    void sendPongs();
    void changeFromUp(RemoteStatusTracker::RemoteState);

    void traceDataTransferDuration(
        const std::chrono::high_resolution_clock::time_point& start,
        const std::chrono::high_resolution_clock::time_point& end,
        const std::string& name);

    std::unique_ptr<AbstractSender> _sender;
    std::unique_ptr<AbstractReceiver<ValuePtrType> > _receiver;

    IRemoteEndpoint<ValuePtrType>* _endpoint;

    std::chrono::milliseconds _artificialJitter;

    RemoteStatusTracker _remoteStatusTracker;
    RemoteStatusTracker::RemoteState _oldState = RemoteStatusTracker::RemoteState::STATE_UNSURE;
    std::deque<uint64_t> _pongQueue;

    mutable std::mutex _mtxS;
};

template <typename ValuePtrType>
RemotePair<ValuePtrType>::RemotePair(
    std::unique_ptr<AbstractSender> sender,
    std::unique_ptr<AbstractReceiver<ValuePtrType> > receiver,
    std::chrono::milliseconds artificialJitter)
: _sender(std::move(sender))
, _receiver(std::move(receiver))
, _artificialJitter(std::move(artificialJitter))
, _remoteStatusTracker([this](uint64_t fv) -> void { _sender->sendPing(fv); })
{
    _sender->connect();
    _receiver->setEventListener(this);
}

template <typename ValuePtrType>
RemotePair<ValuePtrType>::~RemotePair() = default;

template <typename ValuePtrType>
void
RemotePair<ValuePtrType>::connectReceiver(IRemoteEndpoint<ValuePtrType>* endpoint)
{
    _endpoint = endpoint;
    _receiver->connect();
}

template <typename ValuePtrType>
void
RemotePair<ValuePtrType>::connectSender()
{
    _sender->connect();
}

template <typename ValuePtrType>
void
RemotePair<ValuePtrType>::disconnectReceiver()
{
    _endpoint = nullptr;
    _receiver->disconnect();
}

template <typename ValuePtrType>
void
RemotePair<ValuePtrType>::disconnectSender()
{
    _sender->disconnect();
}

template <typename ValuePtrType>
bool
RemotePair<ValuePtrType>::connected() const
{
    return _remoteStatusTracker.getState() == RemoteStatusTracker::RemoteState::STATE_UP;
}

template <typename ValuePtrType>
bool
RemotePair<ValuePtrType>::receive(std::chrono::milliseconds timeout)
{
    bool success = _receiver->receive(timeout);
    if (success && _remoteStatusTracker.getState() == RemoteStatusTracker::STATE_DOWN)
    {
        _remoteStatusTracker.messageReceivedInDown();
    }
    return success;
}

template <typename ValuePtrType>
std::string
RemotePair<ValuePtrType>::sendValue(const std::string& topic, ValuePtr value)
{
    std::lock_guard<std::mutex> lk(_mtxS);
    if(_artificialJitter.count() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(
            std::rand() % (_artificialJitter.count() + 1)));
    }
    auto result = _sender->sendValue(topic, std::move(value));
    if (result == "TIMEOUT")
    {
        _remoteStatusTracker.sendingTimeout();
    }
    return result;
}

template <typename ValuePtrType>
std::string
RemotePair<ValuePtrType>::sendBlockedValueInjected(const std::string& topic)
{
    std::lock_guard<std::mutex> lk(_mtxS);
    auto result = _sender->sendBlockedValueInjected(topic);
    if (result == "TIMEOUT")
    {
        _remoteStatusTracker.sendingTimeout();
    }
    return result;
}

template <typename ValuePtrType>
std::string
RemotePair<ValuePtrType>::sendBlockedValueRejected(const std::string& topic)
{
    std::lock_guard<std::mutex> lk(_mtxS);
    auto result = _sender->sendBlockedValueRejected(topic);
    if (result == "TIMEOUT")
    {
        _remoteStatusTracker.sendingTimeout();
    }
    return result;
}

template <typename ValuePtrType>
void
RemotePair<ValuePtrType>::requestAllReceived()
{
    if (_endpoint) {
        _endpoint->sendAll();
    }
}

template <typename ValuePtrType>
void
RemotePair<ValuePtrType>::sendPongs()
{
    std::lock_guard<std::mutex> lck(_mtxS);
    while (!_pongQueue.empty())
    {
        uint64_t freshnessValue = _pongQueue.front();
        auto start              = std::chrono::high_resolution_clock::now();

        _sender->sendPong(freshnessValue);

        auto end = std::chrono::high_resolution_clock::now();
        traceDataTransferDuration(start, end, fmt::format("send pong {}", freshnessValue));

        _pongQueue.pop_front();
    }
}

template <typename ValuePtrType>
void
RemotePair<ValuePtrType>::pingReceived(uint64_t freshnessValue)
{
    std::lock_guard<std::mutex> lck(_mtxS);
    _pongQueue.push_back(freshnessValue);
    _endpoint->triggerSendCycle();
}

template <typename ValuePtrType>
void
RemotePair<ValuePtrType>::pongReceived(uint64_t freshnessValue)
{
    _remoteStatusTracker.pongReceived(freshnessValue);
}

template <typename ValuePtrType>
std::string
RemotePair<ValuePtrType>::remoteStateString() const
{
    switch (_remoteStatusTracker.getState())
    {
        case RemoteStatusTracker::RemoteState::STATE_DOWN: return "DOWN";
        case RemoteStatusTracker::RemoteState::STATE_UNSURE: return "UNSURE";
        case RemoteStatusTracker::RemoteState::STATE_UP: return "UP";
        default: return "UnknownRemoteState";
    }
}

template <typename ValuePtrType>
void
RemotePair<ValuePtrType>::observeStateChange()
{
    RemoteStatusTracker::RemoteState currState = _remoteStatusTracker.getState();

    if (_oldState != currState)
    {
        MCF_INFO_NOFILELINE("Switching to state {}", remoteStateString());
        if (_oldState == RemoteStatusTracker::RemoteState::STATE_UP)
        {
            changeFromUp(currState);
        }
        _oldState = currState;
    }
}

template <typename ValuePtrType>
void
RemotePair<ValuePtrType>::changeFromUp(const RemoteStatusTracker::RemoteState)
{
    // reset connection
    _sender->disconnect();
    _sender->connect();

    if (_endpoint) {
        _endpoint->resetPendingValues();
    }
}

template <typename ValuePtrType>
void
RemotePair<ValuePtrType>::cycle()
{
    _remoteStatusTracker.runCyclic();
    sendPongs();
}

template <typename ValuePtrType>
void
RemotePair<ValuePtrType>::traceDataTransferDuration(
    const std::chrono::high_resolution_clock::time_point& start,
    const std::chrono::high_resolution_clock::time_point& end,
    const std::string& name)
{
    auto componentTraceEventGenerator = ComponentTraceEventGenerator::getLocalInstance();
    if (componentTraceEventGenerator)
    {
        componentTraceEventGenerator->traceRemoteTransferTime(
            start, end, fmt::format("{} {}", name, remoteStateString()));
    }
}

} // namespace remote
} // namespace mcf

#endif // MCF_REMOTE_REMOTEPAIR_H