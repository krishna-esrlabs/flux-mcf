/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_PORT_H
#define MCF_PORT_H

#include "mcf_core/ValueStore.h"
#include "mcf_core/ValueFactory.h"
#include "mcf_core/IdGeneratorInterface.h"
#include "mcf_core/Messages.h"
#include "mcf_core/IComponent.h"
#include "mcf_core/ComponentTraceEventGenerator.h"
#include "mcf_core/PortTriggerHandler.h"
#include "mcf_core/LoggingMacros.h"

#include <chrono>
#include <thread>
#include <memory>

namespace mcf {

class ComponentManager;

namespace detail
{
/**
 * @brief Control if port access should be synchronized
 *
 */
constexpr bool synchronizePorts = true;
template <typename Mutex>
struct NullLock
{
    explicit NullLock(Mutex& mutex) {}
};

template <typename Mutex>
using Lock = std::conditional_t<synchronizePorts, std::lock_guard<Mutex>, NullLock<Mutex>>;
} // namespace detail

class Port {
public:
    explicit Port(IComponent& component, std::string name)
    : fComponent(component), fValueStore(nullptr), fName(std::move(name)), fConnected(false)
    {}

    /**
     * @brief Move constructor
     *
     * The need for a custom move constructor arises from the following use case
     *
     * std::vector<Port> ports;
     * for (std::size_t i = 0; i < numCameras; ++i)
     * {
     *   ports.push_back(Port<Image>(...))
     * }
     *
     * which happens naturally if you have a custom number of streams which you want to pass over
     * the FLUX interface. In *this specific* use case, there is no need for guaranteeing thread
     * safety because ports are move-constructed in one thread prior to their general availability
     * to other threads.
     */
    Port(Port&& port) noexcept
    : fComponent(port.fComponent)
    , fValueStore(port.fValueStore)
    , fKey(std::move(port.fKey))
    , fName(std::move(port.fName))
    , fConnected(port.fConnected.load())
    {
    }

    /**
     * @brief Copy constructor
     *
     * The copy constructor is explicitly deleted, port are unique resources.
     */
    Port(Port&) = delete;

    /**
     * @brief Copy assignment operator
     *
     * Copy assignment is deleted, see Port::Port(Port&)
     */
    Port& operator=(Port&) = delete;

    virtual ~Port() = default;

    bool isConnected() const {
        return fConnected;
    }

    enum Direction {
        sender,
        receiver
    };

    virtual Direction getDirection() = 0;

    const IComponent& getComponent() const {
        return fComponent;
    }

    std::string getTopic() const {
        detail::Lock<std::mutex> lk(fMutex);
        return fKey;
    }

    std::string getName() const {
        return fName;
    }

    virtual void connect() {
        detail::Lock<std::mutex> lk(fMutex);
        if (!fKey.empty()) {
            connectUnsafe();
        } else {
            // no topic mapped, connect() will have no effect
            MCF_DEBUG_NOFILELINE("Port {} is not mapped to a topic, connect() has no effect", getName());
        }
    }

    virtual void disconnect() {
        detail::Lock<std::mutex> lk(fMutex);
        disconnectUnsafe();
    }

protected:
    friend ComponentManager;

    virtual std::type_index getTypeIndex() = 0;

    virtual void setup(ValueStore& valueStore) {
        detail::Lock<std::mutex> lk(fMutex);
        fValueStore = &valueStore;
    }

    virtual void connectUnsafe() {
        if (fValueStore != nullptr) {
            fConnected = true;
        }
    }

    virtual void disconnectUnsafe() {
        fConnected = false;
    }

    void mapToTopic(const std::string& topic) {
        detail::Lock<std::mutex> lk(fMutex);
        bool wasConnected = fConnected;
        if (wasConnected) {
            disconnectUnsafe();
        }
        fKey = topic;
        if (wasConnected) {
            connectUnsafe();
        }
    }

    virtual void setComponentTraceEventGenerator(
            const std::shared_ptr<ComponentTraceEventGenerator>& eventGenerator)
    {
        fComponentTraceEventGenerator = eventGenerator;
    }

    IComponent& fComponent;
    ValueStore* fValueStore;
    std::string fKey;
    std::string fName;
    std::atomic<bool> fConnected;
    std::shared_ptr<ComponentTraceEventGenerator> fComponentTraceEventGenerator = nullptr;
    /**
     * A mutex object with the following semantics:
     * As long as the mutex is locked, the inner state cannot be changed in a different thread.
     */
    mutable std::mutex fMutex;
};

class GenericReceiverPort : public Port {
public:
    explicit GenericReceiverPort(IComponent& component, const std::string& name) :
        Port(component, name),
        fHandler(nullptr)
    {}

    /*
     * Register a std::function as a handler with component trace event generator.
     *
     * This is a convenience wrapper which creates a Handler object automatically.
     *
     * Note that this way activation flags can not be shared between ports.
     * In order to share the activation flag, create a single Handler object
     * and register it with all the ports.
     */
    void registerHandler(const std::function<void()>& handler) {
        registerHandler(std::make_shared<PortTriggerHandler>(handler, "",
                                                             getComponent().getComponentTraceEventGenerator()));
    }

    /*
     * Register a handler which will be called when the port receives a value.
     *
     * Receive events are represented by a single flag. This means if two events
     * occur with no handler activation in between, the handler will only run once.
     *
     * The activation flag belongs to the Handler object. If the same Handler
     * object is registered with multiple ports, the flag will be shared:
     * If a value comes in on each of the two ports with no handler activation
     * in between, the handler will only be called once.
     */
    void registerHandler(std::shared_ptr<PortTriggerHandler> handler) {
        detail::Lock<std::mutex> lk(fMutex);
        if (fHandler != nullptr) {
            fComponent.unregisterHandler(fHandler);
            if (fValueStore != nullptr && isConnected()) {
                fValueStore->removeReceiver(fKey, fHandler->getEventFlag());
            }
        }
        fHandler = std::move(handler);
        fComponent.registerHandler(fHandler);
        if (fValueStore != nullptr && isConnected()) {
            fValueStore->addReceiver(fKey, fHandler->getEventFlag());
        }
    }

    Direction getDirection() override {
        return receiver;
    }

protected:
    void connectUnsafe() override {
        Port::connectUnsafe();
        if (fValueStore != nullptr && fHandler != nullptr) {
            fValueStore->addReceiver(fKey, fHandler->getEventFlag());
        }
    }

    void disconnectUnsafe() override {
        Port::disconnectUnsafe();
        if (fValueStore != nullptr && fHandler != nullptr) {
            fValueStore->removeReceiver(fKey, fHandler->getEventFlag());
        }
    }

protected:

    void tracePortPeek(const Value* vp) const {
        if (fComponentTraceEventGenerator)
        {
            fComponentTraceEventGenerator->tracePeekPortValue(fKey, fConnected, vp);
        }
    }

    void tracePortAccess(const Value* vp) const {
        if (fComponentTraceEventGenerator)
        {
            fComponentTraceEventGenerator->traceGetPortValue(fKey, fConnected, vp);
        }
    }

private:
    std::shared_ptr<PortTriggerHandler> fHandler;
};


class GenericNonQueuedReceiverPort : public GenericReceiverPort {
public:
    explicit GenericNonQueuedReceiverPort(IComponent& component, const std::string& name) :
        GenericReceiverPort(component, name),
        fEventFlag(std::make_shared<mcf::EventFlag>())
    {}

    /*
     * Returns true if a value as ever been received.
     */
    bool hasValue() const {
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            return fEventFlag->active();
        }
        else {
            return false;
        }
    }

    std::shared_ptr<const Value>getValue() const {
        std::shared_ptr<const Value> vp;
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            vp = std::move(fValueStore->getValue<Value>(fKey));
        }
        else {
            vp = std::make_shared<const Value>();
        }
        tracePortAccess(vp.get());
        return vp;
    }
protected:
    void connectUnsafe() override {
        // add event flag receiver _before_ trigger event receiver
        // this ensures that the event is already there when the
        // handler gets called
        if (fValueStore != nullptr) {
            fValueStore->addReceiver(fKey, fEventFlag);
        }
        GenericReceiverPort::connectUnsafe();
    }

    void disconnectUnsafe() override {
        // remove event flag receiver _after_ trigger event receiver, see connect()
        GenericReceiverPort::disconnectUnsafe();
        if (fValueStore != nullptr) {
            fValueStore->removeReceiver(fKey, fEventFlag);
        }
    }

private:
    std::shared_ptr<EventFlag> fEventFlag;
};


template<typename T>
class ReceiverPort : public GenericNonQueuedReceiverPort {
public:
    explicit ReceiverPort(IComponent& component, const std::string& name) :
        GenericNonQueuedReceiverPort(component, name)
    {}

    std::type_index getTypeIndex() override {
        return std::type_index(typeid(T));
    }


    /*
     * Pop and return the next value from the queue.
      *
     * Silently pops a default instance of the type T if port is not connected
     */
    std::shared_ptr<const T>getValue() const {
        std::shared_ptr<const T> vp;
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            vp = std::move(fValueStore->getValue<T>(fKey));
        }
        else {
            vp = std::make_shared<const T>();
        }
        tracePortAccess(vp.get());
        return vp;
    }
};


class GenericQueuedReceiverPort : public GenericReceiverPort {
public:
    /**
     * Construct a queued receiver port.
     *
     * Arguments:
     *
     *  component   the component this port is part of
     *  queueSize   the maximum length of the queue, 0 means infinite
     *  blocking    if true, a sender on the same topic will
     *              block as long as the queue is full
     */
    GenericQueuedReceiverPort(IComponent& component, const std::string& name, size_t queueSize, bool blocking=false) :
        GenericReceiverPort(component, name),
        fQueue(std::make_shared<mcf::ValueQueue>(queueSize, blocking))
    {}

    bool hasValue() const {
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            return !fQueue->empty();
        }
        else {
            return false;
        }
    }

    size_t getQueueSize() const {
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            return fQueue->size();
        }
        else {
            return 0;
        }
    }

    std::shared_ptr<const Value>peekValue() const {
        std::shared_ptr<const Value> vp;
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            vp = std::move(fQueue->peek<Value>());
        }
        else {
            vp = std::make_shared<const Value>();
        }
        tracePortPeek(vp.get());
        return vp;
    }

    std::shared_ptr<const Value>getValue() const {
        std::shared_ptr<const Value> vp;
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            vp = std::move(fQueue->pop<Value>());
        }
        else {
            vp = std::make_shared<const Value>();
        }
        tracePortAccess(vp.get());
        return vp;
    }

    bool getBlocking() {
        return fQueue->getBlocking();
    }

    void setBlocking(bool blocking) {
        fQueue->setBlocking(blocking);
    }

    size_t getMaxQueueLength() {
        return fQueue->getMaxLength();
    }

    void setMaxQueueLength(size_t maxLength) {
        fQueue->setMaxLength(maxLength);
    }

protected:
    void connectUnsafe() override {
        // add queue receiver _before_ trigger event receiver
        // this ensures that the queue entry is already there when the
        // handler gets called
        if (fValueStore != nullptr) {
            fValueStore->addReceiver(fKey, fQueue);
        }
        GenericReceiverPort::connectUnsafe();
    }

    void disconnectUnsafe() override {
        GenericReceiverPort::disconnectUnsafe();
        // disconnect queue receiver _after_ trigger event receiver, see connect()
        // TODO: this could leave an element in the queue unprocessed
        //       either make (un)registration of receivers atomic or use just one queue
        //       or document properly
        if (fValueStore != nullptr) {
            fValueStore->removeReceiver(fKey, fQueue);
        }
    }

    std::shared_ptr<ValueQueue> fQueue;
};


template<typename T>
class QueuedReceiverPort : public GenericQueuedReceiverPort {
public:
    QueuedReceiverPort(IComponent& component, const std::string& name, size_t queueSize, bool blocking=false) :
        GenericQueuedReceiverPort(component, name, queueSize, blocking)
    {}

    std::type_index getTypeIndex() override {
        return std::type_index(typeid(T));
    }

    /*
     * Get the next value from the queue. The queue is not modified
     *
     * Silently creates a default instance of the type T if port is not connected
     * or the queue is empty. Call hasValue() to check if there are values.
     */
    std::shared_ptr<const T>peekValue() const {
        std::shared_ptr<const T> vp;
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            vp = std::move(fQueue->peek<T>());
        }
        else {
            vp = std::make_shared<const T>();
        }
        tracePortPeek(vp.get());
        return vp;
    }
    /*
     * Pop end return the next value from the queue.
     *
     * Silently creates a default instance of the type T if port is not connected
     * or the queue is empty. Call hasValue() to check if there are values.
     */
    std::shared_ptr<const T>getValue() const {
        std::shared_ptr<const T> vp;
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            vp = std::move(fQueue->pop<T>());
        }
        else {
            vp = std::make_shared<const T>();
        }
        tracePortAccess(vp.get());
        return vp;
    }
};


class GenericSenderPort : public Port {
public:
    explicit GenericSenderPort(IComponent& component, const std::string& name) :
        Port(component, name)
    {}

    /**
     * Writes a ValuePtr to the topic associated with this Port
     *
     * @param vp       A shared pointer to the value that shall be added to the value store
     * @param blocking Binary flag indicating if the function shall block or return immediately
     *                 (and unsuccessfully) if the topic to which the value shall be written is
     *                 currently blocked
     * @param inputIds A vector of value ids which will be written to the trace event.
     * @return An integer indicating either success or an error code:
     *         0:         Success, vp has been written to the value store
     *         EAGAIN:    vp was not written to the value store the topic is currently blocked
     *                    and the argument blocking set to false
     *         ENOTCONN:  The Port is currently not connected an no value has been written
     *         ECANCELED: The Port has been disconnected while setValue() was in progress. May
     *                    happen independently of whether 'blocking' is set to true or false.
     */
    int setValue(ValuePtr vp, bool blocking=true, const std::vector<uint64_t>& inputIds = std::vector<uint64_t>()) {
        int retVal = ENOTCONN;
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected())
        {
            retVal = fValueStore->setValue(fKey, vp, blocking, [this] { return !isConnected(); });
        }
        tracePortAccess(vp.get(), inputIds); // TODO: Since ports may block now until receiver queues are ready,
                                             //       we may want to trace blocking time periods as well
        return retVal;
    }

    Direction getDirection() override {
        return sender;
    }

    void disconnect() override {
        disconnectUnsafe();  // atomically sets fConnected to false => no extra mutex needed,
                             // => any blocked call of setValue() will abort after polling interval
    }

protected:

    void tracePortAccess(const Value* vp, const std::vector<uint64_t>& inputIds) const {
        if (fComponentTraceEventGenerator)
        {
            fComponentTraceEventGenerator->traceSetPortValue(fKey, fConnected, inputIds, vp);
        }
    }
};


template<typename T>
class SenderPort : public GenericSenderPort {
public:
    explicit SenderPort(IComponent& component, const std::string& name) :
        GenericSenderPort(component, name)
    {}

    std::type_index getTypeIndex() override {
        return std::type_index(typeid(T));
    }

    /**
     * See base class
     */
    int setValue(ValuePtr vp, bool blocking=true, const std::vector<uint64_t>& inputIds = std::vector<uint64_t>()) {
        int retVal = ENOTCONN;
        tracePortAccess(vp.get(), inputIds);
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            retVal = fValueStore->setValue(fKey, vp, blocking, [this] { return !isConnected(); });
        }
        return retVal;
    }

    /**
     * Put a value on the value store, overload for unique_ptr<non-const T>.
     *
     * In principle non-immutable values should not be put on the value store. However, if it's a
     * std::unique_ptr, there is no other owner of this value. Hence, we must set a new value id an
     * put the value onto the value store.
     */
    int setValue(
        std::unique_ptr<typename std::remove_cv<T>::type> vp,
        bool blocking                         = true,
        const std::vector<uint64_t>& inputIds = std::vector<uint64_t>())
    {
        int retVal = ENOTCONN;
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            fComponent.idGenerator().injectId(*vp);
            tracePortAccess(vp.get(), inputIds);  // if connected, trace port access after ID generation
            retVal = fValueStore->setValue(fKey, std::shared_ptr<const T>(vp.release()), blocking, [this] { return !isConnected(); });
        }
        else
        {
            tracePortAccess(vp.get(), inputIds);  // if unconnected, trace port access with original value
        }
        return retVal;
    }

    /**
     * Forbidden overload. Putting a non-immutable value to the value store is forbidden.
     *
     * This is not really SFINAE-friendly, but in general this is not a huge problem since there are
     * only very specific types which setValue() can be called with.
     * @return ENOTCONN
     */
    constexpr int setValue(
        const std::shared_ptr<typename std::remove_cv<T>::type>& /* vp */,
        bool /* blocking */                         = true,
        const std::vector<uint64_t>& /* inputIds */ = std::vector<uint64_t>()) noexcept
    {
        static_assert(
            !std::is_same<T, T>::value,
            "Calling setValue() with a pointer to a non-immutable value is not allowed for thread "
            "safety reasons. Consider using immutable values or std::move.");
        return ENOTCONN;
    }

    int setValue(T& value, bool blocking=true, const std::vector<uint64_t>& inputIds = std::vector<uint64_t>()) {
        int retVal = ENOTCONN;
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            fComponent.idGenerator().injectId(value);
            tracePortAccess(&value, inputIds);  // if connected, trace port access after ID generation
            ValuePtr vp = fComponent.valueFactory().createValue(value);
            retVal = fValueStore->setValue(fKey, vp, blocking, [this] { return !isConnected(); });
        }
        else
        {
            tracePortAccess(&value, inputIds);  // if unconnected, trace port access with original value
        }
        return retVal;
    }

    int setValue(T&& value, bool blocking=true, const std::vector<uint64_t>& inputIds = std::vector<uint64_t>()) {
        int retVal = ENOTCONN;
        detail::Lock<std::mutex> lk(fMutex);
        if (isConnected()) {
            fComponent.idGenerator().injectId(value);
            tracePortAccess(&value, inputIds);  // if connected, trace port access after ID generation
            ValuePtr vp = fComponent.valueFactory().createValue(std::forward<T>(value));
            retVal = fValueStore->setValue(fKey, vp, blocking, [this] { return !isConnected(); });
        }
        else
        {
            tracePortAccess(&value, inputIds);  // if unconnected, trace port access with original value
        }
        return retVal;
    }
};

} // namespace mcf

#endif // MCF_PORT_H

