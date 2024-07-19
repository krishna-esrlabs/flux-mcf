/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_ABSTRACTSENDER_H
#define MCF_REMOTE_ABSTRACTSENDER_H

#include "mcf_core/Mcf.h"

#include "mcf_core/ErrorMacros.h"

#include <chrono>

namespace mcf{

namespace remote {

/**
 * Abstract interface for classes intended to implement the sending functionality of
 * RemotePair.
 * Implementations of this class are not considered to be thread safe. In particular, the
 * functions connect, disconnect, as well as all sending functions shall only be called
 * from the same thread (called sending thread in the following documentation).
 */
class AbstractSender
{
public:
    virtual ~AbstractSender() = default;

    /**
     * Establishes a connection using parameters that have been stored previously.
     * After this call has succeeded, the function connected() shall return true until
     * the connection is destroyed by calling disconnect.
     * If this function is called on an already connected AbstractSender, the existing
     * connection may be reset.
     * In case of failure, the function shall throw a std::runtime_error.
     * This function shall only be called from the sending thread.
     */
    virtual void connect() = 0;

    /**
     * If a connection has been established by calling connect it shall be disconnected
     * by this function. If the connection is not established, this function is a no-op.
     * After this function is called, the function connected() shall return false.
     * This function shall only be called from the sending thread.
     */
    virtual void disconnect() = 0;

    /**
     * Sends a Value to a receiver over an implementation defined communication channel.
     * This function shall only be called from the sending thread.

     * @param topic  Topic of the value to be transferred
     * @param value  The value to be transferred
     *
     * @return Returns one of
     *    - TIMEOUT   no response from receiver within timeout
     *    - REJECTED  receiver rejected the value (e.g. because of a missing receive rule)
     *    - RECEIVED  the value was received but not yet injected into the target value store
     *    - INJECTED  the value was received and injected into the target value store
     */
    virtual std::string sendValue(const std::string& topic, ValuePtr value) = 0;

    /**
     * Sends a ping message containing a freshness value to a receiver over an implementation
     * defined communication channel.
     * This function shall only be called from the sending thread.
     *
     * @param freshnessValue  A unique identifier that will be transferred
     */
    virtual void sendPing(uint64_t freshnessValue) = 0;

    /**
     * Sends a pong message containing a freshness value to a receiver over an implementation
     * defined communication channel.
     * This function shall only be called from the sending thread.
     *
     * @param freshnessValue  A unique identifier, typically obtained from a previous ping message,
     *                        that will be transferred
     */
    virtual void sendPong(uint64_t freshnessValue) = 0;

    /**
     * Sends a control message requesing one value (if any are available) on every sendRule from
     * the receiver of this message.
     * This function shall only be called from the sending thread.
     */
    virtual void sendRequestAll() = 0;

    /**
     * Sends a message which indicates that a previously received Value on a certain topic has
     * been injected in the local value store.
     * This function shall only be called from the sending thread.
     *
     * @param topic  The topic on which the previously received Value was injected
     */
    virtual std::string sendBlockedValueInjected(const std::string& topic) = 0;

    /**
     * Sends a message which indicates that a previously received Value on a certain topic has
     * been rejected by the local value store.
     * This function shall only be called from the sending thread.
     *
     * @param topic  The topic on which the previously received Value was rejected
     */
    virtual std::string sendBlockedValueRejected(const std::string& topic) = 0;

    /**
     * Checks if the connect method has been successfully executed.
     *
     * @returns  true if the sender is connected, false otherwise
     */
    virtual bool connected() const = 0;

    /**
     * Returns a string characterizing the connection of this AbstractSender.
     *
     * @returns  A string characterizing the connection of this AbstractSender
     */
    virtual std::string connectionStr() const = 0;

};

} // end namespace remote

} // end namespace mcf

#endif
