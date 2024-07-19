/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_remote/RemoteStatusTracker.h"

#include "mcf_core/ComponentTraceEventGenerator.h"

#include "spdlog/spdlog.h"

#include <random>

namespace mcf{

namespace remote {

RemoteStatusTracker::RemoteStatusTracker(
    std::function<void(uint64_t)> pingSender,
    std::chrono::milliseconds pingInterval,
    std::chrono::milliseconds pingIntervalMax,
    std::chrono::milliseconds pongTimeout
) :
    _remoteState(STATE_UNSURE),
    _pingInterval(pingInterval),
    _pingIntervalMin(pingInterval),
    _pingIntervalMax(pingIntervalMax),
    _pongTimeout(pongTimeout),
    _pingSender(pingSender)
{
    // generate freshness value. The freshness value will be increased every time a new ping is sent
    // random number generation taken from https://en.cppreference.com/w/cpp/numeric/random/uniform_int_distribution
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<uint64_t> distrib;
    _pingFreshnessValue = distrib(gen);
}

RemoteStatusTracker::RemoteStatusTracker(
    RemoteStatusTracker&& other,
    std::function<void(uint64_t)> pingSender)
: _remoteState(other._remoteState)
, _pingInterval(other._pingInterval)
, _pingIntervalMin(other._pingIntervalMin)
, _pingIntervalMax(other._pingIntervalMax)
, _pongTimeout(other._pongTimeout)
, _pingSender(pingSender)
, _lastPingTime(other._lastPingTime)
, _lastPongTime(other._lastPongTime)
, _pingFreshnessValue(other._pingFreshnessValue)
{}

void RemoteStatusTracker::pongReceived(uint64_t freshnessValue)
{
    std::lock_guard<std::mutex> lck(_mtx);

    if(freshnessValue != _pingFreshnessValue) return;

    if(_remoteState == STATE_UNSURE)
    {
        setState(STATE_UP);
    }

    if(_remoteState == STATE_UP)
    {
        _lastPongTime = std::chrono::system_clock::now();
    }
}

void RemoteStatusTracker::runCyclic()
{
    std::lock_guard<std::mutex> lck(_mtx);

    if(_remoteState == STATE_UNSURE)
    {
        if(std::chrono::system_clock::now() > _lastPingTime + _pingInterval)
        {

            sendPing();
            _pingInterval *= 2;
            if(_pingInterval > _pingIntervalMax)
            {
                setState(STATE_DOWN);
            }
        }
    }

    if(_remoteState == STATE_UP)
    {
        if(std::chrono::system_clock::now() > _lastPingTime + _pingInterval)
        {
            sendPing();
            if(std::chrono::system_clock::now() > _lastPongTime + _pongTimeout)
            {
                setState(STATE_DOWN);
            }
        }
    }
}

std::chrono::milliseconds RemoteStatusTracker::pingInterval()
{
    std::lock_guard<std::mutex> lck(_mtx);

    return _pingInterval;
}
void RemoteStatusTracker::messageReceivedInDown()
{
    std::lock_guard<std::mutex> lck(_mtx);

    setState(STATE_UNSURE);
}
void RemoteStatusTracker::sendingTimeout()
{
    std::lock_guard<std::mutex> lck(_mtx);

    setState(STATE_UNSURE);
}

void RemoteStatusTracker::waitForEvent() {
    std::unique_lock<std::mutex> lk(_mtx);
    // couldn't verify if the argument rel_time is evaluated before lock is released
    // by wait_for, so a local copy is made while _mtx is locked
    auto waitMs = std::chrono::milliseconds(_pingInterval);
    _notifierCv.wait_for(lk, waitMs);
}

void RemoteStatusTracker::setState(RemoteState state)
{
    if(state == STATE_UNSURE)
    {
        _pingInterval = std::chrono::milliseconds(_pingIntervalMin);
        _lastPingTime = std::chrono::time_point<std::chrono::system_clock>();
    }
    if(state == STATE_UP)
    {
        _pingInterval = _pingIntervalMax;
    }

    _remoteState = state;

    _notifierCv.notify_all();
}


void RemoteStatusTracker::sendPing()
{
    _lastPingTime = std::chrono::system_clock::now();
    _pingFreshnessValue += 1;

    auto start = std::chrono::high_resolution_clock::now();

    _pingSender(_pingFreshnessValue);

    auto end = std::chrono::high_resolution_clock::now();

    auto componentTraceEventGenerator = ComponentTraceEventGenerator::getLocalInstance();

    if (componentTraceEventGenerator)
    {
        componentTraceEventGenerator->traceRemoteTransferTime(
            start,
            end,
            fmt::format("send ping {}", _pingFreshnessValue));
    }
}

} // end namespace remote

} // end namespace mcf