/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_ABSTRACTRECEIVER_H
#define MCF_REMOTE_ABSTRACTRECEIVER_H

#include "mcf_core/Mcf.h"

#include <chrono>

namespace mcf{

namespace remote {

// forward declarations
template<typename ValuePtrType>
class IComEventListener;

/**
 * Abstract interface for classes intended to implement the receiving functionality of
 * RemotePair.
 * Implementations fo this class are not considered to be thread safe, In particular, the
 * functions connect, disconnect, and receive shall only be called from the same thread
 * (called receiving thread in the following documentation)
 */
template<typename ValuePtrType>
class AbstractReceiver
{
public:
    virtual ~AbstractReceiver() = default;

    /**
     * Stores the event listener passed as a pointer argument. No memory management is performed
     * on that pointer, the ownership remains on the calling side which has to make sure that
     * the listener will outlast the AbstractReceiver holding a pointer to it.
     *
     * @param listener A ComEventListener to which the received messages are forwarded
     */
    void setEventListener(IComEventListener<ValuePtrType>* listener) {_listener = listener;}

    /**
     * Removes a previously set event listener.
     */
    void removeEventListener() {_listener = nullptr;}

    /**
     * Establishes a connection using parameters that have been stored previously.
     * After this call has succeeded, the function connected() shall return true until
     * the connection is destroyed by calling disconnect.
     * If this function is called on an already connected AbstractReceiver, the existing
     * connection may be reset.
     * In case of failure, the function shall throw a std::runtime_error
     * This function shall only be called from the receiving thread
     */
    virtual void connect() = 0;

    /**
     * If a connection has been established by calling connect it shall be disconnected
     * by this function. If the connection is not established, this function is a no-op.
     * After this function is called, the function connected() shall return false.
     * This function shall only be called from the receiving thread
     */
    virtual void disconnect() = 0;

    /**
     * Tries to receive a message from a sender over an implementation defined communication channel
     * and calls the respective _listener functions accordingly.
     * Can have a variable timeout, which may also be zero or infinite (default).
     * This function shall only be called from the receiving thread
     *
     * @param timeout The time in milliseconds this function waits if not receiving anything before
     *                returning
     *
     * @return true if something was received
     *         false if the function timed out or an error occurred
     */
    virtual bool receive(std::chrono::milliseconds timeout) = 0;

    /**
     * Checks if the connect method has been successfully executed
     *
     * @return true if the sender is connected, false otherwise
     */
    virtual bool connected() const = 0;


protected:

    IComEventListener<ValuePtrType>* _listener = nullptr;
};

} // end namespace remote

} // end namespace mcf

#endif
