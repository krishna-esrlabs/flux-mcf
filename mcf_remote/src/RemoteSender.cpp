/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_remote/RemoteSender.h"
#include "mcf_remote/Remote.h"

#include <regex>
#include <random>

namespace mcf {

namespace remote {

RemoteSender::RemoteSender(ValueStore& valueStore, std::shared_ptr<remote::ShmemKeeper> shmemKeeper) :
    mcf::Component("RemoteSender"),
    fValueStore(valueStore),
    fContext(1)
#ifdef HAVE_SHMEM
    , fShmemKeeper(shmemKeeper)
#endif
{
#if PROFILING
    _logger = spdlog::basic_logger_mt(getName(), "logs/remoteSender.log", true);
    _logger->set_level(spdlog::level::trace);
#endif
}

void RemoteSender::configure(mcf::IComponentConfig& config) {
    for (auto& me : fRoutingMap) {
        config.registerPort(*me.second.port, me.first);
        me.second.port->registerHandler(std::bind(&RemoteSender::route, this, me.first));
    }
}

void RemoteSender::route(const std::string& topic) {
    try {
        auto& me = fRoutingMap.at(topic);

        while (me.port->hasValue()) {
            auto value = me.port->getValue();

            PerfLogger startSending("startSending", value->id(), topic, _logger);

            auto typeInfoPtr = fValueStore.getTypeInfo(*value);
            if (typeInfoPtr != nullptr) {

                // if the value is sent over multiple sockets, only
                // the times of the last socket will be recorded
                PerfLogger startPacking("startPacking", value->id(), topic, _logger);
                PerfLogger donePacking("donePacking", value->id(), topic, _logger);
                for (auto socket : me.sockets) {
                    auto sockPtr = socket.first;
                    std::string shmemConnection = socket.second;
                    startPacking.tick();

                    msgpack::sbuffer buffer;
                    msgpack::packer<msgpack::sbuffer> pk(&buffer);
                    pk.pack(topic);

                    zmq::message_t request(buffer.data(), buffer.size());
                    sockPtr->send(request, ZMQ_SNDMORE);

                    if(shmemConnection.empty())
                    {
                        remote::sendValue(value, *typeInfoPtr, *sockPtr);
                    }
                    else
                    {
#ifdef HAVE_SHMEM
                        if(!fShmemKeeper.get())
#endif
                        {
                            log(LogSeverity::err,
                                    fmt::format("No ShmemKeeper was set. Cannot send value on {} over shm://{}",
                                            topic,
                                            shmemConnection)
                                    );
                            continue;
                        }
#ifdef HAVE_SHMEM
                        remote::sendValue(value, *typeInfoPtr, *sockPtr, shmemConnection, fShmemKeeper.get());
#endif
                    }

                    donePacking.tick();

                    zmq::message_t resp;
                    sockPtr->recv(&resp);
                }
            }
            else {
                MCF_ERROR_NOFILELINE("in RemoteSender route: can't serialize unknown type for topic: {}", topic);
            }

            PerfLogger doneSending("doneSending", value->id(), topic, _logger);
        }
    }
    catch (const std::out_of_range& e) {
        // do nothing
    }
}

void RemoteSender::shutdown() {
    for (auto& p : fRequestSockets) {
        p.second->close();
    }
}

void RemoteSender::parseTarget(const std::string& target, std::string& socketName, std::string& shmemFile)
{
    // shm not used, use target as socket name
    if(target.substr(0, 6).compare("shm://"))
    {
        socketName = target;
        return;
    }

    // shm is passed as target
    // generate ipc socket name
    socketName = std::string("ipc:///tmp/") + target.substr(6);
    shmemFile = target.substr(6);
}

} // end namespace remote

} // end namespace mcf

