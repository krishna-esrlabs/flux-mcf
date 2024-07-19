/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_CONTROL_H
#define MCF_REMOTE_CONTROL_H

#include "mcf_core/Mcf.h"
#include "mcf_remote/Remote.h"
#include "zmq.hpp"

#include <ratio>
#include <atomic>
#include <memory>

namespace mcf {

// forward declarations
class ReplayEventController;
class QueuedEventSource;

namespace remote {

/**
 * The RemoteControl class allows various commands to be sent via the python interface in
 * remote_control.py to control MCF using msgpack and zmq. It can:
 *      - Read and write values directly to the value store.
 *      - Connect and disconnect ports.
 *      - Get information about components.
 *      - Control replay playback via the ReplayEventController.
 *      - Manage dynamic events via an event source queue.
 */
class RemoteControl : public Component {

public:
    /**
     * Constructor
     *
     * @param port             The port over which to communicate
     * @param componentManager Reference to the component manager used to query components/ports.
     *                         The reference is stored within this class.
     * @param valueStore       Reference to a ValueStore to write/receive values to/from.
     *                         The reference is stored within this class.
     */
    RemoteControl(int port, ComponentManager& componentManager, ValueStore& valueStore);

    /**
     * Constructor
     *
     * @param port                  The port over which to communicate
     * @param componentManager      Reference to the component manager used to query
     *                              components/ports. The reference is stored within this class.
     * @param valueStore            Reference to a ValueStore to write/receive values to/from.
     *                              The reference is stored within this class.
     * @param replayEventController Controller to be used for event queueing.
     */
    RemoteControl(int port,
                  ComponentManager& componentManager,
                  ValueStore& valueStore,
                  std::shared_ptr<ReplayEventController> replayEventController);

    /**
     * See base class documentation
     */
    void startup() override;

    /**
     * See base class documentation
     */
    void shutdown() override;

    /**
     * Creates a queued event source which can be connected to the ReplayEventController via
     * enableEventQueueing().
     */
    void createRemoteControlEventSource();

    /**
     * Enable or disable the event queueing.
     *
     * @param enabled enable event queueing from remote control
     *                disable event queueing from remote control
     *
     * @return false if the remote control event source has not been created via
     *         createRemoteControlEventSource(), true otherwise
     */
    bool enableEventQueueing(bool enabled);

    /**
     * Set the ReplayEventController which is required for controlling the replay playback or for
     * using the event queuing mechanism.
     *
     * @param replayEventController Controller to be used for event queueing.
     */
    void setReplayEventController(std::shared_ptr<ReplayEventController> replayEventController);

    /**
     * Get the event source which is used for event queuing.
     */
    std::shared_ptr<QueuedEventSource> getRemoteControlEventSource();

private:

    void run();
    void getReplayParams(msgpack::zone& zone);
    void getSimTime(msgpack::zone& zone);
    void setPlaybackModifier(const msgpack::object& request, msgpack::zone& zone);
    void setReplayParams(const msgpack::object& request, msgpack::zone& zone);
    void processRequest(const msgpack::object& request);
    msgpack::object commandGetInfo(const msgpack::object& request, msgpack::zone& zone);
    mcf::PortProxy findPort( const msgpack::object& request, msgpack::zone& zone);
    void connectPort(const msgpack::object& request, msgpack::zone& zone, bool connect);
    void getPortBlocking(const msgpack::object& request, msgpack::zone& zone);
    void setPortBlocking(const msgpack::object& request, msgpack::zone& zone);
    void getPortMaxQueueLength(const msgpack::object& request, msgpack::zone& zone);
    void setPortMaxQueueLength(const msgpack::object& request, msgpack::zone& zone);
    void readValue(const msgpack::object& request, msgpack::zone& zone);
    void writeValue(const msgpack::object& request, msgpack::zone& zone);
    void setQueue(const msgpack::object& request, msgpack::zone& zone);
    msgpack::object commandEventQueueInfo(msgpack::zone& zone);
    void commandEnableEventQueue(const msgpack::object& request, msgpack::zone& zone);
    void sendResponse(const msgpack::object& responseObj, bool sendMore=false);
    void sendErrorResponse(const std::string& message, msgpack::zone& zone);
    void sendEmptyResponse(msgpack::zone& zone, bool sendMore=false);

    template<typename T>
    void sendResponseWithValue(msgpack::zone& zone, bool sendMore, const std::string& key, T value);

    ComponentManager& fComponentManager;
    ValueStore& fValueStore;
    std::shared_ptr<ReplayEventController> fReplayEventController;
    std::shared_ptr<QueuedEventSource> fRemoteControlEventSource;
    int fServerPort;
    zmq::context_t fContext;
    zmq::socket_t fSocket;
    std::map<std::string, std::shared_ptr<mcf::ValueQueue>> fQueueMap;

    std::atomic<bool> fIsEventQueueEnabled;
};


template<typename T>
void RemoteControl::sendResponseWithValue(
        msgpack::zone& zone, bool sendMore, const std::string& key, T value) {
    std::map<std::string, msgpack::object> result;
    result["type"] = msgpack::object("response", zone);
    // empty content
    std::map<std::string, msgpack::object> content;
    content[key] = msgpack::object(value, zone);
    result["content"] = msgpack::object(content, zone);
    sendResponse(msgpack::object(result, zone), sendMore);
}

} // end namespace remote

} // end namespace mcf

#endif
