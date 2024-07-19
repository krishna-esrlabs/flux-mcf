/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_ICOMEVENTLISTENER_H
#define MCF_REMOTE_ICOMEVENTLISTENER_H

#include "mcf_core/Mcf.h"

namespace mcf{

namespace remote {

/**
 * Abstract interface class for communication event listeners like RemotePair.
 *
 * @tparam ValuePtrType Type of the value the wire format deserializes to.
 */
template<typename ValuePtrType>
class IComEventListener
{
public:

    /**
     * Function to be called by an AbstractReceiver when it received a Value.
     * @param topic  The topic to which the value shall be written
     * @param value  A shared_ptr with the received Value
     *
     * @return A string indicating if the received value has been injected in the local value store
     *         There are 3 possible return strings
     *    - REJECTED      receiver rejected the value (e.g. because of a missing receive rule)
     *    - RECEIVED      the value was received but not yet injected into the target value store
     *    - INJECTED      the value was received and injected into the target value store

     */
    virtual std::string valueReceived(const std::string& topic, ValuePtrType value) = 0;

    /**
     * Function to be called by an AbstractReceiver when it received a ping message.
     * @param freshnessValue  The freshnessValue that has been received with the ping
     */
    virtual void pingReceived(uint64_t freshnessValue) = 0;

    /**
     * Function to be called by an AbstractReceiver when it received a pong message.
     * @param freshnessValue  The freshnessValue that has been received with the pong
     */
    virtual void pongReceived(uint64_t freshnessValue) = 0;

    /**
     * Function to be called by an AbstractReceiver when it received a message with a requestAll
     * command.
     */
    virtual void requestAllReceived() = 0;

    /**
     * Function to be called by an AbstractReceiver when it received a message with a valueInjected
     * command.
     * @param topic  The topic to which can be unlocked as the associated value has been injected
     *               on the remote side
     */
    virtual void blockedValueInjectedReceived(const std::string& topic) = 0;

    /**
     * Function to be called by an AbstractReceiver when it received a message with a valueRejected
     * command.
     * @param topic  The topic to which can be unlocked as the associated value has been injected
     *               on the remote side
     */
    virtual void blockedValueRejectedReceived(const std::string& topic) = 0;

};


} // end namespace remote

} // end namespace mcf

#endif