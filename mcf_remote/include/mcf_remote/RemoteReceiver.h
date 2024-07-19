/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_RECEIVER_H
#define MCF_REMOTE_RECEIVER_H


#include "mcf_core/Mcf.h"
#include "mcf_core/PerfLogger.h"

#include "zmq.hpp"
#include "spdlog/spdlog.h"

namespace mcf {

namespace remote {

// forward declarations
class ShmemClient;

/**
 * Class implementing a receiver of mcf Values over 0MQ
 *
 * @warning This is a legacy class. Use RemoteService instead.
 */
class RemoteReceiver : public Component {

public:
    /**
     * Constructor with port number defaults to tcp protocol
     * @param port       The port to be listened to at txp://*:[port]
     * @param valueStore The value store in which the received values will be inserted
     */
    RemoteReceiver(int port, ValueStore& valueStore) :
        RemoteReceiver("tcp://*:"+std::to_string(port), valueStore)
    {}

    /**
     * Constructor with receiver string as argument
     * @param receiver    Description of the connection to be listened too.
     *                    Currently 3 protocols are supported
     *                    - tcp:*:[port]          Listening to a port of all ip-addresses, works
     *                                            locally or remote
     *                    - ipc:///tmp/[filename] Using 0MQ inter process communication. Only local
     *                    - shm://[filename]      Using 0MQ inter process communication for values
     *                                            and a shared memory file for the extmem part of
     *                                            extmem values. Only local
     * @param valueStore  The value store in which the received values will be inserted
     * @param shmemClient The implementation to handle the shared memory reference.
     *                    Only used if with the protocol shm
     */
    RemoteReceiver(
            const std::string& receiver,
            ValueStore& valueStore,
            std::shared_ptr<ShmemClient> shmemClient = nullptr);

    /**
     * See base class
     */
    void configure(IComponentConfig& config) override;

    /**
     * Add a routing rule.
     *
     * MUST be called before ComponentManager configure() call
     *
     * @tparam T The type of the data to be received on this connection
     *
     * @param topic A string identifying this connection
     */
    template<typename T>
    void addReceiveRule(const std::string& topic);

    /**
     * See base class
     */
    void startup() override;

    /**
     * See base class
     */
    void shutdown() override {
        fSocket.close();
    }

private:
    void run();
    void parseTarget(const std::string& target);

    ValueStore& fValueStore;
    std::string fServerPort;
    std::string fShmConnection;
    zmq::context_t fContext;
    zmq::socket_t fSocket;
    std::map<std::string, std::unique_ptr<GenericSenderPort>> fRoutingMap;

    std::shared_ptr<spdlog::logger> _logger;

    std::shared_ptr<ShmemClient> fShmemClient;
};

template<typename T>
void RemoteReceiver::addReceiveRule(const std::string& topic)
{
    fRoutingMap[topic] = std::make_unique<SenderPort<T>>(*this, fmt::format("Receive[{}]", topic));
}

} // end namespace remote

} // end namespace mcf

#endif
