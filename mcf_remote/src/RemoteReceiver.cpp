/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_remote/RemoteReceiver.h"
#include "mcf_remote/Remote.h"

namespace mcf {

namespace remote {

RemoteReceiver::RemoteReceiver(
        const std::string& receiver,
        ValueStore& valueStore,
        std::shared_ptr<remote::ShmemClient> shmemClient) :
    mcf::Component("RemoteReceiver"+receiver),
    fValueStore(valueStore),
    fContext(1),
    fSocket(fContext, ZMQ_REP)
#ifdef HAVE_SHMEM
    , fShmemClient(shmemClient)
#endif
{
    parseTarget(receiver);

#if PROFILING
    _logger = spdlog::basic_logger_mt(getName(), "logs/remoteReceiver.log", true);
    _logger->set_level(spdlog::level::trace);
#endif
}

void RemoteReceiver::configure(IComponentConfig& config) {
    for (auto& me : fRoutingMap) {
        config.registerPort(*me.second, me.first);
    }
}

void RemoteReceiver::startup() {
    registerTriggerHandler(std::bind(&RemoteReceiver::run, this));
    trigger();
    int timeout = 100;  // in milliseconds
    fSocket.setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    fSocket.bind(fServerPort);
}

void RemoteReceiver::run() {
    zmq::message_t request;
    bool success = false;
    try {
        success = fSocket.recv(&request);
    }
    catch (zmq::error_t& e) {
        MCF_WARN_NOFILELINE("in RemoteReceiver recv: {}", e.what());
    }
    if (success) {
        PerfLogger startReceiving("startReceiving", _logger);

        auto oh = msgpack::unpack((const char *)request.data(), request.size());
        auto topic = oh.get().as<std::string>();

        PerfLogger startUnpacking("startUnpacking", _logger);
        // TODO: check if there is another message part
        ValuePtr value = nullptr;
        try {
            if(fShmConnection.empty())
            {
                value = remote::receiveValue(fValueStore, fSocket);
            }
            else
            {
#ifdef HAVE_SHMEM
                if(!fShmemClient.get())
#endif
                {
                    log(LogSeverity::err,
                            fmt::format("No ShmemClient was set. Cannot receive value on {} over shm://{}",
                                    topic,
                                    fShmConnection)
                            );
                    return;
                }
#ifdef HAVE_SHMEM
                value = remote::receiveValue(fValueStore, fSocket, fShmemClient.get());
#endif
            }
        }
        catch (remote::ReceiveError& e) {
            MCF_ERROR_NOFILELINE("In RemoteReceiver receiveValue: receive error: {}", e.what());
        }
        PerfLogger doneUnpacking("doneUnpacking", value->id(), topic, _logger);

        zmq::message_t memresp;
        try {
            fSocket.send(memresp);
        }
        catch (zmq::error_t& e) {
            MCF_ERROR_NOFILELINE("in RemoteReceiver send response: {}", e.what());
        }
        if (value != nullptr) {
            if (fRoutingMap.find(topic) != fRoutingMap.end()) {
                fRoutingMap.at(topic)->setValue(value);
            }
        }

        PerfLogger doneReceiving("doneReceiving", value->id(), topic, _logger);

        startReceiving.setData(value->id(), topic);
        startUnpacking.setData(value->id(), topic);
    }
    trigger();
}

void RemoteReceiver::parseTarget(const std::string& target)
{
    // shm not used, use target as socket name
    if(target.substr(0, 6).compare("shm://"))
    {
        fServerPort = target;
        return;
    }

    // shm is passed as target
    // generate ipc socket name
    fServerPort = std::string("ipc:///tmp/") + target.substr(6);
    fShmConnection = target.substr(6);
}


} // end namespace remote

} // end namespace mcf

