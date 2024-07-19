/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_REMOTE_ABSTRACTZMQMSGPACKRECEIVER_H
#define MCF_REMOTE_ABSTRACTZMQMSGPACKRECEIVER_H

#include "mcf_core/ErrorMacros.h"
#include "mcf_remote/AbstractReceiver.h"
#include "mcf_remote/IComEventListener.h"
#include "mcf_remote/ShmemClient.h"
#include "mcf_remote/ZmqMsgPackUtils.h"
#include "zmq.hpp"

namespace mcf
{
namespace remote
{
namespace
{

/**
 * Class to read/decode messages received over 0MQ and serialized using MessagePack
 *
 * @tparam T The type of the received message after deserialization
 */
template <typename T>
class ZmqMsgPackReader
{
public:
    /**
     * Constructor
     *
     * @param socket The 0MQ socket over which messages shall be received
     */
    explicit ZmqMsgPackReader(zmq::socket_t& socket) : _socket(socket){};

    /**
     * Tries to receive a message over socket. If that is not successful, a zmq::error_t is
     * thrown. If a message could be received, it is unpacked using MessagePack and the result
     * is returned.
     *
     * @return The unpacked message
     */
    T receiveAndUnpackData();

private:
    zmq::socket_t& _socket;
};

template <typename T>
T
ZmqMsgPackReader<T>::receiveAndUnpackData()
{
    zmq::message_t message;
    bool success = _socket.recv(&message);
    if (!success)
        throw zmq::error_t();

    auto oh = msgpack::unpack((const char*)message.data(), message.size());
    return oh.get().as<T>();
}

template <>
std::string
ZmqMsgPackReader<std::string>::receiveAndUnpackData()
{
    zmq::message_t message;
    bool success = _socket.recv(&message);
    if (!success)
        throw zmq::error_t();

    std::string result;
    // msgpack produces an error unpacking strings which contain only the terminating
    // character:
    // msgpack-c/include/msgpack/v2/create_object_visitor.hpp:119:24:
    // runtime error: null pointer passed as argument 2, which is declared to never be null
    // Therefore we avoid unpacking such strings
    if (message.size() > 1)
    {
        auto oh = msgpack::unpack((const char*)message.data(), message.size());
        result  = oh.get().as<std::string>();
    }

    return result;
}

} // namespace

/**
 * Implements the functions connect, disconnect, receive, and connected of AbstractReceiver.
 * It adds the abstract function decodeValue to be implemented by subclasses.
 *
 * @tparam ValuePtrType The datatype that is encoded in the 0MQ messages to be received.
 */
template <typename ValuePtrType>
class AbstractZmqMsgPackReceiver : public AbstractReceiver<ValuePtrType>
{
public:
    explicit AbstractZmqMsgPackReceiver(
        const std::string& connection, std::shared_ptr<ShmemClient> shmemClient = nullptr);

    virtual ~AbstractZmqMsgPackReceiver() = default;

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
    bool
    receive(const std::chrono::milliseconds timeout) override;

    /**
     * Checks if _socketRec is set, which it should be after the connect function has been called
     * until the disconnect function is called.
     *
     * @returns false if _socketRec is a nullptr, true otherwise.
     */
    bool connected() const override { return _socketRec != nullptr; }

private:
    /**
     * Function to decode the received message. To support different kind of functions
     * (e.g. mcf values, binary blobs, etc.), this function is abstract so it can be implemented
     * for different data types in derived classes.
     *
     * @param message A 0MQ message
     *
     * @return The decoded message
     */
    virtual ValuePtrType decodeValue(ZmqMessage& message) = 0;

    void receivePing();
    void receivePong();
    void receiveCommand();
    void receiveValue();

    /**
     * Receives a zmq message serialized using msgpack and unpacks it to the desired type.
     * The function throws an exception if either receiving or unpacking fails.
     * @warning only call this function when there is something to receive on the socket as it
     *          will block until something has been received.
     *
     * @return The received, unpacked data
     */
    template <typename T>
    T receiveAndUnpackData()
    {
        return ZmqMsgPackReader<T>(*_socketRec).receiveAndUnpackData();
    }

    void sendResponse(const std::string& response = "");

    zmq::context_t _context;
    std::string _connection;
    std::unique_ptr<zmq::socket_t> _socketRec;

    // for access to shared memory segment for inter process communication
    std::string _shmemFileName;
    std::shared_ptr<ShmemClient> _shmemClient;
};

template <typename ValuePtrType>
AbstractZmqMsgPackReceiver<ValuePtrType>::AbstractZmqMsgPackReceiver(
    const std::string& connection, std::shared_ptr<ShmemClient> shmemClient)
: _context(1), _shmemClient(std::move(shmemClient))
{
    parseConnectionName(connection, _connection, _shmemFileName);

    int policy = 0;
    sched_param parameters{-1};
    pthread_getschedparam(pthread_self(), &policy, &parameters);

    if (policy == SCHED_FIFO)
    {
        int priority = 36;
        void* ptr    = (void*)_context;
        zmq_ctx_set(ptr, ZMQ_THREAD_SCHED_POLICY, SCHED_FIFO);
        int res = zmq_ctx_set(ptr, ZMQ_THREAD_PRIORITY, priority);

        // setting a thread name prefix only work with zmq 4.3 and higher
        // int prefix = std::stoi(_connection.substr(_connection.size()-4));
        // res = zmq_ctx_set(ptr, ZMQ_THREAD_NAME_PREFIX, prefix);

        if (res != 0)
        {
            MCF_THROW_RUNTIME(fmt::format(
                "ERROR: cannot set ZMQ_THREAD_PRIORITY to '{}': {}", priority, strerror(errno)));
        }
        else
        {
            MCF_INFO_NOFILELINE(
                "SET ZmqMsgPackReceiver scheduling parameters: policy {}, priority {}",
                policy,
                priority);
        }
    }
    else
    {
        MCF_INFO_NOFILELINE("Cannot change scheduling of ZmqMsgPackReceiver");
    }
}

template <typename ValuePtrType>
void
AbstractZmqMsgPackReceiver<ValuePtrType>::connect()
{
    try
    {
        _socketRec = std::make_unique<zmq::socket_t>(_context, ZMQ_REP);
        // int timeout = 100;    // in milliseconds
        // _socketRec->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));

        _socketRec->bind(_connection);
    }
    catch (const zmq::error_t& e)
    {
        _socketRec.reset();
        MCF_THROW_RUNTIME(fmt::format(
            "ERROR: ZmqMsgPackReceiver cannot connect to '{}': {}", _connection, e.what()));
    }
}

template <typename ValuePtrType>
void
AbstractZmqMsgPackReceiver<ValuePtrType>::disconnect()
{
    _socketRec.reset();
}

template <typename ValuePtrType>
void
AbstractZmqMsgPackReceiver<ValuePtrType>::receivePing()
{
    uint64_t freshnessValue = 0u;

    try
    {
        freshnessValue = receiveAndUnpackData<uint64_t>();
    }
    catch (zmq::error_t& e)
    {
        MCF_WARN_NOFILELINE("in ZmqMsgPackReceiver receivePing: {}", e.what());
        return;
    }

    sendResponse();

    if (this->_listener)
        this->_listener->pingReceived(freshnessValue);
}

template <typename ValuePtrType>
void
AbstractZmqMsgPackReceiver<ValuePtrType>::receivePong()
{
    uint64_t freshnessValue = 0u;

    try
    {
        freshnessValue = receiveAndUnpackData<uint64_t>();
    }
    catch (zmq::error_t& e)
    {
        MCF_WARN_NOFILELINE("in ZmqMsgPackReceiver receivePong: {}", e.what());
    }

    sendResponse();

    if (this->_listener)
        this->_listener->pongReceived(freshnessValue);
}

template <typename ValuePtrType>
void
AbstractZmqMsgPackReceiver<ValuePtrType>::receiveCommand()
{
    try
    {
        std::string command = receiveAndUnpackData<std::string>();

        if (command == "sendAll")
        {
            sendResponse();
            if (this->_listener)
                this->_listener->requestAllReceived();
            return;
        }

        if (command == "valueInjected")
        {
            std::string topic = receiveAndUnpackData<std::string>();
            sendResponse();
            if (this->_listener)
                this->_listener->blockedValueInjectedReceived(topic);
            return;
        }

        if (command == "valueRejected")
        {
            std::string topic = receiveAndUnpackData<std::string>();
            sendResponse();
            if (this->_listener)
                this->_listener->blockedValueRejectedReceived(topic);
            return;
        }

        MCF_ERROR_NOFILELINE("ZmqMsgPackReceiver received unsupported command: {}", command);
    }
    catch (zmq::error_t& e)
    {
        MCF_WARN_NOFILELINE("in ZmqMsgPackReceiver receiveCommand: {}", e.what());
        return;
    }
}

template <typename ValuePtrType>
void
AbstractZmqMsgPackReceiver<ValuePtrType>::receiveValue()
{
    zmq::message_t request;
    try
    {
        const std::string topic = receiveAndUnpackData<std::string>();

        ValuePtrType value = ValuePtrType();
        ZmqMsgPackMessageReceiver(*_socketRec, _shmemClient.get(), _shmemFileName)
            .receive(topic, [this, &topic](ZmqMessage& message) {
                ValuePtrType value = this->decodeValue(message);
                if (value != nullptr && this->_listener)
                {
                    std::string retVal = this->_listener->valueReceived(topic, value);
                    sendResponse(retVal);
                }
                else
                {
                    sendResponse("REJECTED");
                }
            });
    }
    catch (std::exception& e)
    {
        MCF_ERROR_NOFILELINE("In RemoteService receiveValue: {}", e.what());
    }
}

template <typename ValuePtrType>
bool
AbstractZmqMsgPackReceiver<ValuePtrType>::receive(const std::chrono::milliseconds timeout)
{
    MCF_ASSERT(connected(), "trying to receive before ZmqMsgPackReceiver was connected");

    zmq_pollitem_t item;
    item.socket = *_socketRec;
    item.events = ZMQ_POLLIN;
    int rc      = zmq_poll(&item, 1, timeout.count());

    if (rc == 0)
    {
        return false;
    }

    std::string kind;
    try
    {
        kind = receiveAndUnpackData<std::string>();
    }
    catch (zmq::error_t& e)
    {
        MCF_WARN_NOFILELINE("in ZmqMsgPackReceiver receive: {}", e.what());
        return false;
    }

    if (kind == "ping")
    {
        receivePing();
    }
    else if (kind == "pong")
    {
        receivePong();
    }
    else if (kind == "command")
    {
        receiveCommand();
    }
    else if (kind == "value")
    {
        receiveValue();
    }
    else
    {
        MCF_ERROR_NOFILELINE("RemoteService received unexpected message kind: {}", kind);
    }

    return true;
}

template <typename ValuePtrType>
void
AbstractZmqMsgPackReceiver<ValuePtrType>::sendResponse(const std::string& response)
{
    try
    {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> pk(&buffer);
        pk.pack(response);

        zmq::message_t memresp(buffer.data(), buffer.size());
        _socketRec->send(memresp);
    }
    catch (zmq::error_t& e)
    {
        MCF_ERROR_NOFILELINE("in RemoteService send response: {}", e.what());
    }
}

} // namespace remote
} // namespace mcf

#endif // MCF_REMOTE_ABSTRACTZMQMSGPACKRECEIVER_H