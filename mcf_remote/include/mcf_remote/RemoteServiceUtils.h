/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_SERVICE_UTILS_H
#define MCF_REMOTE_SERVICE_UTILS_H

#include "mcf_remote/RemoteService.h"
#include "mcf_remote/ZmqMsgPackSender.h"
#include "mcf_remote/ZmqMsgPackValueReceiver.h"

namespace mcf
{
namespace remote
{
/**
 * Constructs a RemoteService using 0MQ for communication and msgpack for value serialization.
 *
 * @param connectionSend    A string representing the 0MQ connection over which values are sent to
 *                          remote side
 * @param connectionReceive A string representing the 0MQ connection over which values can be
 *                          received from the remote side
 * @param valueStore        The value store from which values are sent and in which received values
 *                          are inserted
 * @param shmemKeeper       Class to manage shared memory in case it is used. Only needs to be set
 *                          if the connectionSend uses the shm protocol. Using the default argument
 *                          will result in errors if the shm protocol is used
 * @param shmemClient       Helper class to give reading access to a shared memory file.
 *                          Only used if the protocol shm is specified in connectionReceive.
 *                          Using the default argument will result in errors if the shm protocol is
 *                          used
 * @param sendTimeout       Time in milliseconds the system waits for an ack after a send before
 *                          the send function returns TIMEOUT
 * @param artificialJitter  Maximum amount of jitter that is artificially added to simulate a slower
 *                          connection
 *
 * @return A shared_ptr to the constructed RemoteService
 */
inline std::shared_ptr<mcf::remote::RemoteService>
buildZmqRemoteService(
    const std::string& connectionSend,
    const std::string& connectionReceive,
    ValueStore& valueStore,
    std::shared_ptr<ShmemKeeper> shmemKeeper = nullptr,
    std::shared_ptr<ShmemClient> shmemClient = nullptr,
    std::chrono::milliseconds sendTimeout = std::chrono::milliseconds(100),
    std::chrono::milliseconds artificialJitter = std::chrono::milliseconds(0))
{
    std::unique_ptr<AbstractSender> sender(new mcf::remote::ZmqMsgPackSender(
        connectionSend, valueStore, sendTimeout, shmemKeeper));

    std::unique_ptr<AbstractReceiver<ValuePtr>> receiver(
        new mcf::remote::ZmqMsgPackValueReceiver(connectionReceive, valueStore, shmemClient));

    return std::make_shared<mcf::remote::RemoteService>(
        valueStore, 
        RemotePair<ValuePtr>(std::move(sender), std::move(receiver), artificialJitter));
}

} // end namespace remote

} // end namespace mcf

#endif