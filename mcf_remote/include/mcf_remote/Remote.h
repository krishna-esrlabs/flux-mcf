/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_REMOTE_H
#define MCF_REMOTE_H

#include "mcf_core/Mcf.h"
#include "mcf_remote/SerializedValue.h"
#include "zmq.hpp"
#include <mutex>

namespace mcf {

namespace remote {

class SendError  : public std::exception {
public:
    SendError(std::string error = "")
    {
        fError = "SendError. " + error;
    }

    virtual const char* what() const throw() {
        return fError.c_str();
    }

private:

    std::string fError;
};

class ReceiveError  : public std::exception {
public:

    ReceiveError(std::string error = "")
    {
        fError = "ReceiveError. " + error;
    };

    virtual const char* what() const throw() {
        return fError.c_str();
    };

private:

    std::string fError;
};

/**
 * @brief A structure to encapsulate a (complete) message transferred over 0MQ
 *
 * This data structure does not own the message parts and should not be copied outside of its scope.
 */
struct ZmqMessage
{
    /// Copying is disallowed
    ZmqMessage(ZmqMessage&) = delete;
    /// Moving is disallowed
    ZmqMessage(ZmqMessage&&) = delete;

    /// The 0MQ message
    zmq::message_t& request;
    /// The extmem pointer
    const void* extMem;
    /// Length of extmem
    const std::size_t extMemSize;
};

namespace impl {

template <typename F>
void sendValueBase(
       ValuePtr value,
       const TypeRegistry::TypemapEntry& typeInfo,
       zmq::socket_t& socket,
       bool sendMore,
       F& extmemHandling)
{
    using namespace std::placeholders;

    static thread_local msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);

    pk.pack(value->id());
    pk.pack(typeInfo.id);

    const void* ptr;
    size_t len;

    typeInfo.packFunc(pk, value, ptr, len, true);

    zmq::message_t request(buffer.data(), buffer.size());

    socket.send(request, ptr != NULL ? ZMQ_SNDMORE : 0);

    if (ptr != NULL) {
        extmemHandling(value, socket, sendMore, ptr, len);
    }
    buffer.clear();
}

ValuePtr unpackMessage(
        TypeRegistry& typeRegistry,
        zmq::message_t& request,
        const void* ptr,
        size_t len);

SerializedValue getSerializedMessage(zmq::message_t& request, const void* p, size_t len);

template <typename F>
void
receiveMessageBase(
    zmq::socket_t& socket,
    F& extmemHandling,
    const std::function<void(ZmqMessage&)>& messageHandler)
{
    zmq::message_t request;
    try
    {
        if (!socket.recv(&request))   // TODO: handle timeout?
        {
            throw ReceiveError("socket.recv error");
        }
    }
    catch (zmq::error_t& e)
    {
        throw ReceiveError("zmq::error_t: " + std::string(e.what()));
    }

    // try to receive ext mem message part
    bool haveExtMemMessagePart = true;
    zmq::message_t memreq;
    try
    {
        if (!socket.recv(&memreq))  // TODO: handle timeout?
        {
            haveExtMemMessagePart = false;
        }
    }
    catch (zmq::error_t& e)
    {
        haveExtMemMessagePart = false;
    }

    const void* ptr;
    size_t len;
    if (haveExtMemMessagePart)
    {
        extmemHandling(memreq, ptr, len);
    }
    else
    {
        len = 0;
        ptr = nullptr;
    }

    ZmqMessage message{request, ptr, len};
    messageHandler(message);
}

} // end namespace impl

/**
 * Used by the sender to keep shared pointers alive until sending is done.
 */
class ValueKeeper {
public:

    typedef const void* HandleType;
    /**
     * Add a pointer to a Value to the list of pointers to keep.
     *
     * @param value ValuePtr that should not be deleted
     * @return a handle to be used for removing it later
     */
    HandleType addValue(const ValuePtr& value);

    /**
     * Remove shared pointer identified by the handle returned by addValue from the list
     * of shared pointers that should not be deleted.
     * @param handle The handle obtained from the function addValue
     */
    void removeValue(HandleType handle);

    /**
     * 0MQ compatible free function. This function shall be passed as argument free_fn to the
     * constructor call of a zmq::message_t.
     *
     * @param data A pointer to the allocated data (unused)
     * @param hint A handle to the entry that shall be removed from in fMap
     */
    static void zmqFreeFunction(void* data, void* hint);

    /**
     * Set the number of entries in ValueKeeper map
     *
     * @return number of entries in ValueKeeper map
     */
    int getSize() { return fMap.size(); };

private:
    std::map<HandleType, ValuePtr> fMap;
    std::mutex fMutex;
};

// forward declarations

/**
 * Sends a Value to a receiver over a socket using messagepack (for serialization) and
 * 0MQ (to transfer).
 *
 * @param value    The value to be transferred
 * @param typeInfo Type information indicating the actual (sub)type of value
 * @param socket   The socket to be used for data transfer
 * @param sendMore A flag indicating if more data will be appended to the current communication
 */
extern void sendValue(
    ValuePtr value,
    const TypeRegistry::TypemapEntry& typeInfo,
    zmq::socket_t& socket,
    bool sendMore=false);

/**
 * Receives a Value from a sender over a socket using messagepack (for serialization)
 * and 0MQ (to transfer)
 *
 * @param typeRegistry A registry holding type information that can be retrieved with a string
 *                     containing the type name
 * @param socket       The socket do be used to receive data
 *
 * @return A ValuePtr of to the received Value
 */
extern ValuePtr receiveValue(TypeRegistry& typeRegistry, zmq::socket_t& socket);

/**
 * @brief Receives a message blob (0MQ message + extmem) over the 0MQ socket
 *
 * @param messageHandler A handler of a locally allocated value message that should decode and pass the message on
 * @param socket ZeroMQ connection
 */
extern void
receiveMessage(const std::function<void(ZmqMessage&)>& messageHandler, zmq::socket_t& socket);

#ifdef HAVE_SHMEM

// versions using shared memory
class ShmemKeeper;
class ShmemClient;

/**
 * sends a Value to a receiver over a socket using messagepack (for serialization) and 0MQ
 * (to transfer) and boost shared memory to transfer the ExtMem part of ExtMemValues
 *
 * @param Value       The value to be transferred
 * @param typeInfo    Type information indicating the actual (sub)type of value
 * @param socket      The socket to be used for data transfer of the non-ExtMem part of the value
 * @param connection  A string naming the shared memory channel to be used for transfering
 *                    the ExtMem part of the value. Unused if a non-ExtMemValue is transferred
 * @param shmemKeeper Pointer to a data structure that owns the shared memory used to transfer
 *                    ExtMem parts of values. Unused if a non-ExtMemValue is transferred
 * @param sendMore    A flag indicating if more data will be appended to the current communication
 */
extern void sendValue(
    ValuePtr value,
    const TypeRegistry::TypemapEntry& typeInfo,
    zmq::socket_t& socket,
    const std::string& connection,
    ShmemKeeper* shmemKeeper,
    bool sendMore=false);

/**
 * receives a Value from a sender over a socket using messagepack (for serialization) and 0MQ
 * (to transfer) and boost shared memory to transfer the ExtMem part of ExtMemValues
 * @param typeRegistry A registry holding type information that can be retrieved with a string
 *                     containing the type name
 * @param socket       The socket do be used to receive data
 * @param shmemClient  Pointer to a class that provides the functionality to open and read from
 *                     boost shared memory provided a string that identifies the shared memory
 *                     file. Unused if a non-ExtMemValue is transferred
 *
 * @return A ValuePtr of to the received Value
 */
extern ValuePtr receiveValue(
    TypeRegistry& typeRegistry,
    zmq::socket_t& socket,
    ShmemClient* shmemClient);

/**
 * @brief Receives a message blob (0MQ message + extmem) over the 0MQ socket and Boost shared
 *        memory
 *
 * @param messageHandler A handler of a locally allocated value message that should decode and pass
 *                       the message on
 * @param socket         0MQ connection
 * @param shmemClient    Boost shared memory connection
 */
extern void receiveMessage(
    const std::function<void(ZmqMessage&)>& messageHandler,
    zmq::socket_t& socket,
    ShmemClient* shmemClient);

#endif

} // end namespace remote

} // end namespace mcf

#endif
