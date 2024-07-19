/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_SENDER_H
#define MCF_REMOTE_SENDER_H


#include "mcf_core/Mcf.h"
#include "mcf_core/PerfLogger.h"

#include "zmq.hpp"
#include "spdlog/spdlog.h"

namespace mcf {

namespace remote {

// forward declarations
class ShmemKeeper;

/**
 * Class implementing a sender of mcf Values over 0MQ
 *
 * @warning This is a legacy class. Use RemoteService instead.
 */
class RemoteSender : public mcf::Component {

public:
    /**
     * Constructor
     * @param valueStore  The value store in which the received values will be inserted
     * @param shmemKeeper Class to manage shared memory in case it is used. Only needs to be set if a send rule using
     *                    the shm protocol is added. Using the default argument will result in errors if a send rule
     *                    uses the shm protocol
     */
    RemoteSender(
            ValueStore& valueStore,
            std::shared_ptr<ShmemKeeper> shmemKeeper = std::shared_ptr<ShmemKeeper>());

    void configure(mcf::IComponentConfig& config);

    /**
     * Add a routing rule.
     *
     * MUST be called before ComponentManager configure() call
     * @param topic  The topic whose values shall be forwarded to target
     * @param target A string describing the zmq connection that shall be created
     *            Currently 3 protocols are supported for connectionSend and connectionRec
     *            - tcp://[ip]:[port]     Using tcp communication protocol, works locally (using
     *                                    either 'localhost' or '127.0.0.1') or remote
     *            - ipc:///tmp/[filename] Using 0MQ inter process communication. Only local
     *            - shm://[filename]      Using 0MQ inter process communication for values and a
     *                                    shared memory file for the extmem part of extmem values.
     *                                    Only local
     * @param queueLength
     */
    template<typename T>
    void addSendRule(const std::string& topic, const std::string& target, const size_t queueLength = 1);

    /**
     * See base class
     */
    void shutdown() override;

private:

    void route(const std::string& topic);

    struct RoutingMapEntry {
        RoutingMapEntry() :
            port(nullptr)
        {}
        std::unique_ptr<mcf::GenericQueuedReceiverPort> port;
        std::vector<std::pair<std::shared_ptr<zmq::socket_t>, std::string>> sockets;
    };

    void parseTarget(const std::string& target, std::string& socketName, std::string& shmemFile);

    ValueStore& fValueStore;
    std::map<std::string, RoutingMapEntry> fRoutingMap;
    std::map<std::string, std::shared_ptr<zmq::socket_t>> fRequestSockets;
    zmq::context_t fContext;

    std::shared_ptr<spdlog::logger> _logger;

    // for access to shared memory segment for inter process communication
    std::shared_ptr<ShmemKeeper> fShmemKeeper;
};

template<typename T>
void RemoteSender::addSendRule(const std::string& topic, const std::string& target, const size_t queueLength)
{
    std::shared_ptr<zmq::socket_t> sockPtr;
    std::string socketName, shmemFile;
    parseTarget(target, socketName, shmemFile);
    try {
        sockPtr = fRequestSockets.at(socketName);
    }
    catch (std::out_of_range& e) {
        sockPtr = std::make_shared<zmq::socket_t>(fContext, ZMQ_REQ);
        sockPtr->connect(socketName);
        fRequestSockets[socketName] = sockPtr;
    }

    auto& me = fRoutingMap[topic];
    me.sockets.push_back(std::make_pair(sockPtr, shmemFile));
    if (me.port == nullptr) {
        me.port = std::make_unique<QueuedReceiverPort<T>>(*this, fmt::format("Send[{}]", topic), queueLength);
    }
}

} // end namespace remote

} // end namespace mcf

#endif
