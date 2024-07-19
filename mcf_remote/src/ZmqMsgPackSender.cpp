/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_remote/ZmqMsgPackSender.h"
#include "mcf_remote/ZmqMsgPackUtils.h"
#include "mcf_remote/Remote.h"
#include "mcf_remote/ShmemKeeper.h"

namespace mcf {

namespace remote {

ZmqMsgPackSender::ZmqMsgPackSender(
    std::string connection,
    TypeRegistry& typeRegistry,
    std::chrono::milliseconds sendTimeout,
    std::shared_ptr<ShmemKeeper> shmemKeeper) :
    _context(1),
    _connectionStr(std::move(connection)),
    _typeRegistry(typeRegistry),
    _sendTimeout(sendTimeout),
    _shmemKeeper(std::move(shmemKeeper))
{
    parseConnectionName(_connectionStr, _connection, _shmemName);

    int policy = 0;
    sched_param parameters{-1};
    pthread_getschedparam(pthread_self(), &policy, &parameters);

    if(policy == SCHED_FIFO)
    {
        int priority =  35;
        void* ptr = (void*)_context;
        zmq_ctx_set(ptr, ZMQ_THREAD_SCHED_POLICY, SCHED_FIFO);
        int res = zmq_ctx_set(ptr, ZMQ_THREAD_PRIORITY, priority);

        // setting a thread name prefix only work with zmq 4.3 and higher
        //int prefix = std::stoi(_connectionStr.substr(_connectionStr.size()-4));
        //res = zmq_ctx_set(ptr, ZMQ_THREAD_NAME_PREFIX, prefix);

        if(res != 0)
        {
            MCF_THROW_RUNTIME(
                fmt::format(
                    "ERROR: cannot set ZMQ_THREAD_PRIORITY to '{}': {}",
                    priority,
                    strerror(errno)
                )
            );
        }
        else
        {
            MCF_INFO_NOFILELINE("SET ZmqMsgPackSender scheduling parameters: policy {}, priority {}",
                policy,
                priority
            );
        }
    }
    else
    {
        MCF_INFO_NOFILELINE("Cannot change scheduling of ZmqMsgPackSender");
    }
}

void ZmqMsgPackSender::connect()
{
    try
    {
        _socketSend = std::make_unique<zmq::socket_t>(_context, ZMQ_REQ);
        const int one = 1;
        // enabling sending another message even though the response of the previous one did not arrive (yet)
        _socketSend->setsockopt(ZMQ_REQ_RELAXED, &one, sizeof(int));
        // _socketSend->setsockopt(ZMQ_REQ_CORRELATE, &one, sizeof(int));
        // allow destruction of port/context even if there are still some message in flight
        _socketSend->setsockopt(ZMQ_LINGER, &one, sizeof(int));

        _socketSend->connect(_connection);
    }
    catch(const zmq::error_t& e)
    {
        _socketSend.reset();
        MCF_THROW_RUNTIME(
            fmt::format(
                "ERROR: ZmqMsgPackSender cannot connect to '{}': {}",
                _connection,
                e.what()
            )
        );
    }
}

void ZmqMsgPackSender::disconnect()
{
    _socketSend->close();
    _socketSend.reset();
}

std::string ZmqMsgPackSender::sendValue(const std::string& topic, ValuePtr value)
{
    MCF_ASSERT(connected(), "trying to send a Value before ZmqMsgPackSender was connected");

    auto typeInfoPtr = _typeRegistry.getTypeInfo(*value);

    if (typeInfoPtr != nullptr)
    {
        transferData("value", ZMQ_SNDMORE);

        transferData(topic, ZMQ_SNDMORE);

        if(_shmemName.empty())
        {
            remote::sendValue(value, *typeInfoPtr, *_socketSend);
        }
        else
        {
#ifdef HAVE_SHMEM
            if(!_shmemKeeper.get())
#endif
            {
                MCF_ERROR_NOFILELINE(
                        "No ShmemKeeper was set. Cannot send value on {} over shm://{}",
                                topic,
                                _shmemName
                        );
                return "REJECTED";
            }
#ifdef HAVE_SHMEM
            remote::sendValue(value, *typeInfoPtr, *_socketSend, _shmemName, _shmemKeeper.get());
#endif
        }

        std::string response = checkForResponse(_sendTimeout);

        return response;
    }

    return "REJECTED";
}

void ZmqMsgPackSender::sendPing(uint64_t freshnessValue)
{
    MCF_ASSERT(connected(), "trying to send a Ping before ZmqMspPackSender was connected");

    // send a ping signal to the other end to let them know we are here
    transferData("ping", ZMQ_SNDMORE);
    transferData(freshnessValue);

    // check if the ping has been received
    if(checkForResponse(_sendTimeout) == "TIMEOUT")
    {
        MCF_WARN_NOFILELINE("Ping {} timed out", freshnessValue);
    }
}

void ZmqMsgPackSender::sendPong(uint64_t freshnessValue)
{
    MCF_ASSERT(connected(), "trying to send a Pong before ZmqMspPackSender was connected");

    // send a pong signal to the other end to let them know we are here
    transferData("pong", ZMQ_SNDMORE);
    transferData(freshnessValue);

    // check if the pong has been received
    if(checkForResponse(_sendTimeout) == "TIMEOUT")
    {
        MCF_WARN_NOFILELINE("pong {} timed out", freshnessValue);
    }
}

void ZmqMsgPackSender::sendRequestAll()
{
    MCF_ASSERT(connected(), "trying to send a Command before ZmqMspPackSender was connected");

    transferData("command", ZMQ_SNDMORE);
    transferData("sendAll");

    // check if command has been received
    checkForResponse(_sendTimeout);
}

std::string ZmqMsgPackSender::sendBlockedValueInjected(const std::string& topic)
{
    MCF_ASSERT(connected(), "trying to send a Command before ZmqMspPackSender was connected");

    transferData("command", ZMQ_SNDMORE);
    transferData("valueInjected", ZMQ_SNDMORE);
    transferData(topic);

    // check if command has been received
    return checkForResponse(_sendTimeout);
}

std::string ZmqMsgPackSender::sendBlockedValueRejected(const std::string& topic)
{
    MCF_ASSERT(connected(), "trying to send a Command before ZmqMspPackSender was connected");

    transferData("command", ZMQ_SNDMORE);
    transferData("valueRejected", ZMQ_SNDMORE);
    transferData(topic);

    // check if command has been received
    return checkForResponse(_sendTimeout);
}


template<typename T>
void ZmqMsgPackSender::transferData(const T& message, const int flags)
{
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    pk.pack(message);

    // make sure the socket is ready to send data
    zmq::message_t resp;
    zmq_pollitem_t item;
    item.socket = *_socketSend;
    item.events = ZMQ_POLLOUT;
    int rc = zmq_poll(&item, 1, 0);
    if(rc == 0)
    {
        // try to clear a dangling response from the socket
        checkForResponse(std::chrono::milliseconds(100));
        MCF_ASSERT(zmq_poll(&item, 1, 0) != 0, "socket is not ready for sending");
    }

    zmq::message_t request(buffer.data(), buffer.size());
    _socketSend->send(request, flags);
}

std::string ZmqMsgPackSender::checkForResponse(const std::chrono::milliseconds& timeout)
{
    zmq::message_t resp;
    zmq_pollitem_t item;
    item.socket = *_socketSend;
    item.events = ZMQ_POLLIN;
    int rc = zmq_poll(&item, 1, timeout.count());
    if(rc)
    {
        try
        {
            bool result = _socketSend->recv(&resp);
            if(result == false) throw zmq::error_t();
        }
        catch (zmq::error_t& e)
        {
            MCF_WARN_NOFILELINE("in ZmqMsgPackSender checkForResponse: {}", e.what());
            return "REJECTED";
        }

        try
        {
            std::string result;

            // msgpack produces an error unpacking strings which contain only the terminating
            // character:
            // msgpack-c/include/msgpack/v2/create_object_visitor.hpp:119:24:
            // runtime error: null pointer passed as argument 2, which is declared to never be null
            // Therefore we avoid unpacking such strings
            if(resp.size() > 1)
            {
                auto oh = msgpack::unpack((const char *)resp.data(), resp.size());
                result = oh.get().as<std::string>();
            }
            else
            {
                result = "";
            }

            return result;
        }
        catch(const std::exception& e)
        {
            MCF_WARN_NOFILELINE("in ZmqMsgPackSender checkForResponse: {}", e.what());
            return "REJECTED";
        }

    }

    return "TIMEOUT";
}


} // end namespace remote

} // end namespace mcf
