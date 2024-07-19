/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_ZMQMSGPACKRUTILS_H
#define MCF_REMOTE_ZMQMSGPACKRUTILS_H

#include <string>
#include "mcf_remote/Remote.h"
#include "mcf_remote/ShmemClient.h"

namespace mcf{

namespace remote {

/**
 * Checks if the connection string has the prefix shm:// and extracts the socketName and shmemFile
 * if so. If a connection string with another prefix is passed as argument, this function is a
 * no-op.
 *
 * @param connection The name of the connection to be parsed
 * @param socketName Will be filled with ""ipc:///tmp/" + shmemFile" if connection has the prefix
 *                   shm://, unused otherwise
 * @param shmFile    Will be filled the postfix of connection if connection has the prefix
 *                   shm://, unused otherwise
 */
void parseConnectionName(
    const std::string& connection,
    std::string& socketName,
    std::string& shmemFile);

/**
 * Class designed to receive/unpack messages over 0MQ that were serialized using MessagePack.
 * Additional, unserialized, extMem data may be passed along with the message over boost shared
 * memory.
 */
class ZmqMsgPackMessageReceiver
{
public:
    /**
     * Constructor
     * @param socket        A ZMQ_REP socket over which mesages shall be received
     * @param shmemClient   Pointer to a helper class to manage shared memory. A copy of the
     *                      pointer is stored in this class but ownership resides with the calling
     *                      entity.
     * @param shmemFileName String representing the name of the shared memory file to be used. The
     *                      reference of the string is stored in this class but ownership resides with
     *                      calling entity.
     */
    ZmqMsgPackMessageReceiver(
        zmq::socket_t& socket, ShmemClient* shmemClient, const std::string& shmemFileName)
    : _socket(socket), _shmemClient(shmemClient), _shmemFileName(shmemFileName){};

    /**
     * Receives a message and calls the handleMessage function to decode it. The handleMessage
     * function is responsible for making the make the message available, e.g. by writing the
     * decoded message to an mcf port.
     *
     * @param topic         String specifying the connection on which a message shall be received
     * @param handleMessage Function which shall be used for reading the message and make the
     *                      deserialized value accessible.
     */
    void
    receive(const std::string& topic, const std::function<void(ZmqMessage&)>& handleMessage);

private:
    zmq::socket_t& _socket;
    ShmemClient* _shmemClient;
    const std::string& _shmemFileName;
};

} // end namespace remote

} // end namespace mcf

#endif
