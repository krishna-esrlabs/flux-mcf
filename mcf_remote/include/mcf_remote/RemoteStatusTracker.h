/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_REMOTESTATUSTRACKER_H
#define MCF_REMOTE_REMOTESTATUSTRACKER_H

#include <chrono>
#include <functional>
#include <mutex>
#include <condition_variable>

namespace mcf{

namespace remote {

/**
 * Class to keep track of the assumed state of the other side of a RemotePair.
 * Pings are sent and the RemoteState is changed depending on if Pongs are received from the
 * other side.
 * In STATE_DOWN, no pings are sent out. If a ping or another message is received, the state is
 * changed to STATE_UNSURE.
 * In STATE_UNSURE, the ping interval will be doubled on every ping, starting with pingInterval.
 * When the ping interval reaches pingIntervalMax and no matching pong was received, the state is
 * changed to STATE_DOWN. If a pong is received, the state is changed to STATE_UP.
 * In STATE_UP the ping frequency is decreased to 1/pingIntervalMax. If a ping is not answered by
 * a pong, the state is changed to STATE_DOWN. If the function sendingTimeout is called, the state
 * is changed to STATE_UNSURE.
 */
class RemoteStatusTracker
{
public:
    /**
     * Assumed state of the other side. Depending on whether the other side is responding to
     * messages or not, the RemoteService enters one of the following stages and acts
     * accordingly:
     * DOWN:   Assuming the other side is not active. Don't forward any Values.
     *         Don't send any messages, listen to messages from the other side.
     * UNSURE: State of the other side is uncertain, Don't forward any Values.
     *         Actively send ping messages and wait for responses and listen to messages from
     *         the other side
     * UP:     Assuming the other side is up and ready to receive messages. Forward values to
     *         the other side and listen for the responses and other messages form the other side.
     *         Actively send ping messages at a low frequency and listen for responses
     */
    enum RemoteState
    {
        STATE_DOWN   = 0,
        STATE_UNSURE = 1,
        STATE_UP     = 2
    };

    /**
     * Constructor
     *
     * @param pingSender      Function to call when sending a ping
     * @param pingInterval    Initial ping interval.
     * @param pingIntervalMax Maximal ping interval.
     * @param pongTimeout     Time to wait for a pong until the other side is considered to be
     *                        not responding.
     */
    explicit RemoteStatusTracker(
        std::function<void(uint64_t)> pingSender,
        std::chrono::milliseconds pingInterval = std::chrono::milliseconds(100),
        std::chrono::milliseconds pingIntervalMax = std::chrono::milliseconds(3000),
        std::chrono::milliseconds pongTimeout = std::chrono::milliseconds(5000));


    /**
     * Constructor
     *
     * @param other      Source from which everything but the ping sender is copied
     * @param pingSender Function to call when sending a ping
     */
    RemoteStatusTracker(RemoteStatusTracker&& other, std::function<void(uint64_t)> pingSender);

    /**
     * Query the current remoteState
     *
     * @return The current _remoteState
     */
    RemoteState getState() const { return _remoteState; }

    /**
     * Function to be called if a pong is received. The freshnessValue is compared to the ones
     * sent out with the most recent ping to verify the latest ping was answered.
     *
     * @param freshnessValue The freshnessValue that was received with the pong message
     */
    void pongReceived(uint64_t freshnessValue);

    /**
     * Function responsible for sending pings and check if pong-timeouts have occurred.
     * This function should be called on regularly by an external caller.
     */
    void runCyclic();

    /**
     * Query the current ping interval, which varies depending on the state and the pongs received.
     *
     * @return The current interval for sending out ping messages.
     */
    std::chrono::milliseconds pingInterval();

    /**
     * This function shall be called if a message is received while in STATE_DOWN. It will change
     * the state to STATE_UNSURE
     */
    void messageReceivedInDown();

    /**
     * This function shall be called if a timeout on any sent message (not only ping/pong messages)
     * has occurred. It will change the state to STATE_UNSURE.
     */
    void sendingTimeout();

    /**
     * Blocks the calling thread until an event is triggered in the RemoteStatusTracker.
     * This event is either as change of the remote state or the expiration of _pingInterval
     */
    void waitForEvent();

private:
    void setState(RemoteState state);
    void sendPing();

    RemoteState _remoteState;
    std::chrono::milliseconds _pingInterval;
    std::chrono::milliseconds _pingIntervalMin;
    std::chrono::milliseconds _pingIntervalMax;
    std::chrono::milliseconds _pongTimeout;
    std::chrono::time_point<std::chrono::system_clock> _lastPingTime; // set to 01.01.1970
    std::chrono::time_point<std::chrono::system_clock> _lastPongTime; // set to 01.01.1970
    std::function<void(uint64_t)> _pingSender;
    uint64_t _pingFreshnessValue;

    std::mutex _mtx;
    std::condition_variable _notifierCv;
};

} // end namespace remote

} // end namespace mcf

#endif