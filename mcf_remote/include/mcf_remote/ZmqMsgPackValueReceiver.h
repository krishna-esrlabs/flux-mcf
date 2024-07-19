/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_ZMQMSGPACKVALUERECEIVER_H
#define MCF_REMOTE_ZMQMSGPACKVALUERECEIVER_H

#include "mcf_remote/AbstractZmqMsgPackReceiver.h"

namespace mcf
{
class TypeRegistry;
class Value;

using ValuePtr = std::shared_ptr<const Value>;

namespace remote
{
class ShmemClient;

/**
 * Implements the decodeValue function of the abstract baseclass to decode 0MQ messages
 * using MessagePack for serialization.
 */
class ZmqMsgPackValueReceiver : public AbstractZmqMsgPackReceiver<ValuePtr>
{
public:
    /**
     * Constructs a new ZmqMsgPackValueReceiver and stores the passed arguments.
     * The connection will not be established until the function connect() is called
     *
     * @param connection  A string describing the zmq connection that shall be created
     *            Currently 3 protocols are supported for connectionSend and connectionRec
     *            - tcp://[ip]:[port]     Using tcp communication protocol, works locally (using
     *                                    either 'localhost' or '127.0.0.1') or remote
     *            - ipc:///tmp/[filename] Using 0MQ inter process communication. Only local
     *            - shm://[filename]      Using 0MQ inter process communication for values and a
     *                                    shared memory file for the extmem part of extmem values.
     *                                    Only local
     * @param typeRegistry Reference to a class holding the type information of the Value
     *                     types that will be transferred (Typically the ValueStore)
     * @param shmemClient  Helper class to give reading access to a shared memory file.
     *                     Only used if the protocol shm is specified in connection.  Using the
     *                     default argument will result in errors if the shm protocol is used
     */
    ZmqMsgPackValueReceiver(
        const std::string& connection,
        TypeRegistry& typeRegistry,
        std::shared_ptr<ShmemClient> shmemClient = nullptr);

    virtual ~ZmqMsgPackValueReceiver() = default;

private:
    /**
     * Unpacks the received message and constructs a ValuePtr from it.
     *
     * @param message The received ZmqMessage
     *
     * @return A ValuePtr constructed from message
     */
    virtual ValuePtr decodeValue(ZmqMessage& message) override;

    TypeRegistry& _typeRegistry;
};

} // end namespace remote

} // end namespace mcf

#endif // MCF_REMOTE_ZMQMSGPACKVALUERECEIVER_H