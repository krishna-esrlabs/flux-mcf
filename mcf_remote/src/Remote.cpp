/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_remote/Remote.h"
#ifdef HAVE_SHMEM
#include "mcf_remote/ShmemHandlers.h"
#endif
#include "mcf_core/ErrorMacros.h"

#include <iostream>

namespace mcf {

namespace remote {

// single value keeper instance to be accessed by static free function
ValueKeeper valueKeeper;

ValueKeeper::HandleType ValueKeeper::addValue(const ValuePtr& value) {
    std::lock_guard<std::mutex> lk(fMutex);
    auto handle = reinterpret_cast<HandleType>(value.get());
    fMap[handle] = value;
    return handle;
}

void ValueKeeper::removeValue(HandleType handle) {
    std::lock_guard<std::mutex> lk(fMutex);
    auto it = fMap.find(handle);
    if (it != fMap.end()) {
        fMap.erase(it);
    }
};

void ValueKeeper::zmqFreeFunction(void* data, void* hint) {
    (void)data;
    valueKeeper.removeValue(hint);
}

namespace impl {

class IdInjector : public IidGenerator
{
public:
    IdInjector(uint64_t id) : _id(id)
    { }
    virtual void injectId(Value& value) const override
    {
        setId(value, _id);
    }
private:
    const uint64_t _id;
};

ValuePtr unpackMessage(
        TypeRegistry& typeRegistry,
        zmq::message_t& request,
        const void* ptr,
        size_t len)
{
    msgpack::unpacker pac;

    // feeds the buffer.
    pac.reserve_buffer(request.size());
    memcpy(pac.buffer(), request.data(), request.size());
    pac.buffer_consumed(request.size());

    // now starts streaming deserialization.
    msgpack::object_handle oh;

    uint64_t id = 0ul;
    pac.next(oh);
    try // some values may come without a valid id (e.g. from pyton via RemoteControl)
    {
        id = oh.get().as<uint64_t>();
        pac.next(oh);
    }
    catch(const std::exception&)
    {
        // use the DefaultIdGenerator and a dummy Value to generate an id
        DefaultIdGenerator dig;
        Value val;
        dig.injectId(val);
        id = val.id();
    }

    auto classname = oh.get().as<std::string>();

    pac.next(oh);
    msgpack::object o = oh.get();

    auto typeinfoPtr = typeRegistry.getTypeInfo(classname);
    if (typeinfoPtr == nullptr) {
        throw ReceiveError(
            fmt::format("Type of received message not present in type registry: {}", classname));
    }

    bool isExtMem;

    Value* value;
    try
    {
        value = typeinfoPtr->unpackFunc(o, ptr, len, isExtMem);
        IdInjector idInjector(id);
        idInjector.injectId(*value);
    }
    catch (msgpack::v1::type_error& e)
    {
        throw ReceiveError("msgpack::v1::type_error: " + std::string(e.what()));
    }

    return std::shared_ptr<const Value>(value);
}

SerializedValue
getSerializedMessage(zmq::message_t& request, const void* p, size_t len)
{
    // copy buffer, copy extmem, return wire format
    auto dataBuffer = std::make_unique<char[]>(request.size());
    std::memcpy(dataBuffer.get(), request.data(), request.size());

    std::unique_ptr<char[]> extMemBuffer = nullptr;
    size_t extMemSize  = 0;
    bool extMemPresent = p != nullptr;
    if (extMemPresent)
    {
        extMemSize   = len;
        extMemBuffer = std::make_unique<char[]>(extMemSize);
        std::memcpy(extMemBuffer.get(), p, len);
    }
    return SerializedValue(
        std::move(dataBuffer), request.size(), std::move(extMemBuffer), extMemSize);
}

} // end namespace impl

void sendValue(ValuePtr value, const TypeRegistry::TypemapEntry& typeInfo, zmq::socket_t& socket, bool sendMore) {

    auto extmemHandling = [](ValuePtr& value, zmq::socket_t& socket, bool sendMore, const void* ptr, size_t len)
            {
                // add value to the keeper so it won't get destroyed while sending is still in progress
                auto handle = valueKeeper.addValue(value);
                // this message_t constructor doesn't copy the data, it uses zmq_msg_init_data() internally
                // zmq calls the free function once sending is done
                // we need to cast the const away here since zmq only supports void*
                zmq::message_t memreq(const_cast<void*>(ptr), len, ValueKeeper::zmqFreeFunction, const_cast<void*>(handle));
                socket.send(memreq, sendMore ? ZMQ_SNDMORE : 0);
            };

    impl::sendValueBase(
       value,
       typeInfo,
       socket,
       sendMore,
       extmemHandling);
}


ValuePtr receiveValue(TypeRegistry& typeRegistry, zmq::socket_t& socket) {
    auto extmemHandling =
            [](zmq::message_t& memreq, const void*& ptr, size_t& len)
            {
                len = memreq.size();
                ptr = memreq.data();
            };

    ValuePtr value;
    impl::receiveMessageBase<decltype(extmemHandling)>(
        socket,
        extmemHandling,
        [&typeRegistry, &value](ZmqMessage& message) {
            value = impl::unpackMessage(typeRegistry, message.request, message.extMem, message.extMemSize);
        });

    return value;
}

void
receiveMessage(const std::function<void(ZmqMessage&)>& messageHandler, zmq::socket_t& socket)
{
    auto lambda = [](zmq::message_t& memreq, const void*& ptr, size_t& len) {
        len = memreq.size();
        ptr = memreq.data();
    };

    impl::receiveMessageBase<decltype(lambda)>(socket, lambda, messageHandler);
}

#ifdef HAVE_SHMEM

void sendValue(
        ValuePtr value,
        const TypeRegistry::TypemapEntry& typeInfo,
        zmq::socket_t& socket,
        const std::string& connection,
        ShmemKeeper* shmemKeeper,
        bool sendMore)
{
    auto extmemHandling = [&shmemKeeper, &connection](
        ValuePtr& value,
        zmq::socket_t& socket,
        bool sendMore,
        const void* ptr,
        size_t len)
        {
            void* shmemPtr = shmemKeeper->createOrGetPartitionPtr(connection, len);
            if(!shmemPtr)
            {
                throw SendError("Could not allocate shared memory to send value");
                return;
            }
            memcpy(shmemPtr, ptr, len);
            msgpack::sbuffer buffer;
            msgpack::packer<msgpack::sbuffer> pk(&buffer);
            pk.pack(shmemKeeper->shmemFileName(connection));
            pk.pack(shmemKeeper->partitionHandle(connection));
            pk.pack(len);
            zmq::message_t memreq(buffer.data(), buffer.size());

            socket.send(memreq, sendMore ? ZMQ_SNDMORE : 0);
        };

    impl::sendValueBase(
       value,
       typeInfo,
       socket,
       sendMore,
       extmemHandling);
}

void
extMemShmemHandler(ShmemClient* shmemClient, zmq::message_t& memreq, const void*& ptr, size_t& len)
{
    msgpack::unpacker pac;
    // feeds the buffer.
    pac.reserve_buffer(memreq.size());
    memcpy(pac.buffer(), memreq.data(), memreq.size());
    pac.buffer_consumed(memreq.size());
    msgpack::object_handle oh;
    pac.next(oh);
    std::string shmName = oh.get().as<std::string>();
    pac.next(oh);
    bip::managed_shared_memory::handle_t handle
        = oh.get().as<bip::managed_shared_memory::handle_t>();
    ptr = shmemClient->partitionPtr(shmName, handle);
    pac.next(oh);
    len = oh.get().as<size_t>();
}

ValuePtr receiveValue(TypeRegistry& typeRegistry, zmq::socket_t& socket, ShmemClient* shmemClient) {
    auto lambda = [&shmemClient](zmq::message_t& memreq, const void*& ptr, size_t& len) {
        extMemShmemHandler(shmemClient, memreq, ptr, len);
    };

    ValuePtr value;
    impl::receiveMessageBase<decltype(lambda)>(
        socket, lambda, [&typeRegistry, &value](ZmqMessage& message) {
            value = impl::unpackMessage(typeRegistry, message.request, message.extMem, message.extMemSize);
        });

    return value;
}

void
receiveMessage(
    const std::function<void(ZmqMessage&)>& messageHandler,
    zmq::socket_t& socket,
    ShmemClient* shmemClient)
{
    auto lambda = [&shmemClient](zmq::message_t& memreq, const void*& ptr, size_t& len) {
        extMemShmemHandler(shmemClient, memreq, ptr, len);
    };

    impl::receiveMessageBase<decltype(lambda)>(socket, lambda, messageHandler);
}

#endif

} // end namespace remote

} // end namespace mcf

