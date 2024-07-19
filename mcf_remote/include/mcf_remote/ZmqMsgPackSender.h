/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_ZMQMSGPACKSENDER_H
#define MCF_REMOTE_ZMQMSGPACKSENDER_H

#include "zmq.hpp"

#include "mcf_remote/AbstractSender.h"

namespace mcf{

namespace remote {

class ShmemKeeper;

/**
 * Implementation of AbstractSender that uses 0MQ for message sending and MessagePack for data
 * serialization. If the shm protocol is used, additional, unserialized, extMem data may be passed
 * along with the message over boost shared memory.
 */
class ZmqMsgPackSender : public AbstractSender
{
public:
    /**
     * Constructs a new ZmqMsgPackSender and stores the passed arguments.
     * The connection will not be established until the function connect() is called
     *
     * @param connection A string describing the zmq connection that shall be created
     *            Currently 3 protocols are supported for connectionSend and connectionRec
     *            - tcp://[ip]:[port]     Using tcp communication protocol, works locally (using
     *                                    either 'localhost' or '127.0.0.1') or remote
     *            - ipc:///tmp/[filename] Using 0MQ inter process communication. Only local
     *            - shm://[filename]      Using 0MQ inter process communication for values and a
     *                                    shared memory file for the extmem part of extmem values.
     *                                    Only local
     * @param typeRegistry A type registry to query the type info for msgpack serialization
     * @param sendTimeout  Time in milliseconds the system waits for an ack after a send before
     *                     the send function returns TIMEOUT
     * @param shmemKeeper  Class to manage shared memory in case it is used. Only needs to be set
     *                     if the connection uses the shm protocol. Using the default argument
     *                     will result in errors if the shm protocol is used
     */
    ZmqMsgPackSender(
        std::string connection,
        TypeRegistry& typeRegistry,
        std::chrono::milliseconds sendTimeout = std::chrono::milliseconds(100),
        std::shared_ptr<ShmemKeeper> shmemKeeper = nullptr);

    /*
     * See base class
     */
    void connect() override;

    /*
     * See base class
     */
    void disconnect() override;

    /*
     * See base class
     */
    std::string sendValue(const std::string& topic, ValuePtr value) override;

    /*
     * See base class
     */
    void sendPing(uint64_t freshnessValue) override;

    /*
     * See base class
     */
    void sendPong(uint64_t freshnessValue) override;

    /*
     * See base class
     */
    void sendRequestAll() override;

    /*
     * See base class
     */
    std::string sendBlockedValueInjected(const std::string& topic) override;

    /*
     * See base class
     */
    std::string sendBlockedValueRejected(const std::string& topic) override;

    /*
     * See base class
     */
    bool connected() const override { return _socketSend != nullptr; }

    /*
     * See base class
     */
    std::string connectionStr() const override { return _connectionStr; };

private:
    template<typename T>
    void transferData(const T& message, const int flags = 0);

    /**
     * Tries to receive a response to a previously sent message containing a zmq serialized string,
     * unpacks and returns that string.
     * The function will block until something has been received or the timeout has been reached.
     *
     * @return The string contained in the response. In case of a timeout, the string 'TIMEOUT' is
     *         returned
     */
    std::string checkForResponse(const std::chrono::milliseconds& timeout);

    zmq::context_t _context;
    std::string _connectionStr;
    std::string _connection;
    std::unique_ptr<zmq::socket_t> _socketSend;

    TypeRegistry& _typeRegistry;
    const std::chrono::milliseconds _sendTimeout;

    // for access to shared memory segment for inter process communication
    std::string _shmemName;
    std::shared_ptr<ShmemKeeper> _shmemKeeper;

};

} // end namespace remote

} // end namespace mcf

#endif