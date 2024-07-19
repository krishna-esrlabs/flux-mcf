/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_remote/RemoteControl.h"
#include "mcf_core/TimestampType.h"
#include "mcf_core/QueuedEventSource.h"
#include "mcf_core/ReplayEventController.h"
#include "mcf_core/EventTimingController.h"
#include "mcf_core/LoggingMacros.h"
#include "mcf_core/ErrorMacros.h"

namespace mcf {

namespace remote {

RemoteControl::RemoteControl(int port, ComponentManager& componentManager, ValueStore& valueStore)
        : Component("RemoteControl"+std::to_string(port)),
        fComponentManager(componentManager),
        fValueStore(valueStore),
        fServerPort(port),
        fContext(1),
        fSocket(fContext, ZMQ_REP),
        fIsEventQueueEnabled(false)
{}

RemoteControl::RemoteControl(int port,
                             ComponentManager& componentManager,
                             ValueStore& valueStore,
                             std::shared_ptr<ReplayEventController> replayEventController)
        : Component("RemoteControl"+std::to_string(port)),
        fComponentManager(componentManager),
        fValueStore(valueStore),
        fServerPort(port),
        fContext(1),
        fSocket(fContext, ZMQ_REP),
        fReplayEventController(std::move(replayEventController)),
        fIsEventQueueEnabled(false)
{}

void RemoteControl::startup()
{
    registerTriggerHandler([this] { run(); });
    trigger();
    int timeout = 100;
    fSocket.setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    fSocket.bind("tcp://*:"+std::to_string(fServerPort));
}

void RemoteControl::shutdown() {
    fSocket.close();
}

void RemoteControl::sendResponse(const msgpack::object& responseObj, bool sendMore) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    pk.pack(responseObj);

    zmq::message_t response(buffer.data(), buffer.size());
    try {
        fSocket.send(response, sendMore ? ZMQ_SNDMORE : 0);
    }
    catch (zmq::error_t& e) {
        std::cout << "ERROR: in RemoteControl send: " << e.what() << std::endl;
    }
}

void RemoteControl::sendEmptyResponse(msgpack::zone& zone, bool sendMore) {
    std::map<std::string, msgpack::object> result;
    result["type"] = msgpack::object("response", zone);
    // empty content
    std::map<std::string, msgpack::object> content;
    result["content"] = msgpack::object(content, zone);
    sendResponse(msgpack::object(result, zone), sendMore);
}


void RemoteControl::sendErrorResponse(const std::string& message, msgpack::zone& zone) {
    std::map<std::string, msgpack::object> result;
    result["type"] = msgpack::object("error", zone);
    result["content"] = msgpack::object(message, zone);
    sendResponse(msgpack::object(result, zone));
}


void RemoteControl::run() {
    zmq::message_t request;
    int len = 0;
    try {
        len = fSocket.recv(&request);
    }
    catch (zmq::error_t& e) {
        // TODO: handle EINTR
        MCF_ERROR_NOFILELINE("On receive: {}", e.what());
    }
    if (len > 0) {
        msgpack::unpacker pac;

        // feeds the buffer.
        pac.reserve_buffer(request.size());
        memcpy(pac.buffer(), request.data(), request.size());
        pac.buffer_consumed(request.size());

        msgpack::object_handle oh;

        pac.next(oh);
        msgpack::object requestObj = oh.get();
        processRequest(requestObj);
    }
    trigger();
}

msgpack::object RemoteControl::commandGetInfo(const msgpack::object& request, msgpack::zone& zone) {
    (void)request;
    std::vector<msgpack::object> compDescs;
    for (auto c : fComponentManager.getComponents()) {
        std::map<std::string, msgpack::object> compDesc;
        compDesc["name"] = msgpack::object(c.name(), zone);
        compDesc["id"]   = msgpack::object(c.id(), zone);
        std::vector<msgpack::object> portDescs;
        for (auto p : fComponentManager.getPorts(c)) {
            std::map<std::string, msgpack::object> portDesc;
            portDesc["topic"] = msgpack::object(p.topic(), zone);
            portDesc["name"]  = msgpack::object(p.name(), zone);
            portDesc["direction"] = msgpack::object(p.direction() == Port::sender ? "sender" : "receiver", zone);
            portDesc["connected"] = msgpack::object(p.isConnected(), zone);
            portDescs.emplace_back(portDesc, zone);
        }
        compDesc["ports"] = msgpack::object(portDescs, zone);
        compDescs.emplace_back(compDesc, zone);
    }
    return { compDescs, zone };
}


mcf::PortProxy RemoteControl::findPort(
        const msgpack::object& request, msgpack::zone& zone) {
    auto map = request.as<std::map<std::string, msgpack::object>>();
    if (map.find("component") != map.end()) {
        if (map.find("port") != map.end()) {
            auto comps = fComponentManager.getComponents();
            size_t compIdx = map["component"].as<size_t>();
            if (compIdx < comps.size()) {
                auto ports = fComponentManager.getPorts(comps[compIdx]);
                size_t portIdx = map["port"].as<size_t>();
                if (portIdx < ports.size()) {
                    return ports[portIdx];
                }
                else {
                    sendErrorResponse("port not found", zone);
                }
            }
            else {
                sendErrorResponse("component not found", zone);
            }
        }
        else {
            sendErrorResponse("no port given", zone);
        }
    }
    else {
        sendErrorResponse("no component given", zone);
    }
    throw std::runtime_error("Could not find port");
}

void RemoteControl::connectPort(
        const msgpack::object& request, msgpack::zone& zone, bool connect) {
    try {
        mcf::PortProxy port = findPort(request, zone);
        if (connect) {
            port.connect();
        }
        else {
            port.disconnect();
        }
        sendEmptyResponse(zone);
    } catch (std::runtime_error& e) {
        // error message has been sent by findPort
    }
}

void RemoteControl::getPortBlocking(
        const msgpack::object& request, msgpack::zone& zone) {
    try {
        mcf::PortProxy port = findPort(request, zone);
        if (port.isQueued()) {
            bool blocking = port.isBlocking();
            std::map<std::string, msgpack::object> result;
            result["type"] = msgpack::object("response", zone);
            std::map<std::string, msgpack::object> content;
            content["value"] = msgpack::object(blocking, zone);
            result["content"] = msgpack::object(content, zone);
            sendResponse(msgpack::object(result, zone), false);
        }
        else {
            sendErrorResponse("not a queued receiver port", zone);
        }        
    } catch (std::runtime_error& e) {
        // error message has been sent by findPort
    }
}

void RemoteControl::setPortBlocking(
        const msgpack::object& request, msgpack::zone& zone) {
    mcf::PortProxy port = findPort(request, zone);
    try {
        mcf::PortProxy port = findPort(request, zone);
        auto map = request.as<std::map<std::string, msgpack::object>>();
        if (map.find("value") != map.end()) {
            bool blocking = map["value"].as<bool>();
            if (port.isQueued()) {
                port.setBlocking(blocking);
                sendEmptyResponse(zone);
            }
            else {
                sendErrorResponse("not a queued receiver port", zone);
            }
        }
        else {
            sendErrorResponse("no value given", zone);
        }
    }
    catch (std::runtime_error& e) {
        // error message has been sent by findPort
    }
}

void RemoteControl::getPortMaxQueueLength(
        const msgpack::object& request, msgpack::zone& zone) {
    try {
        mcf::PortProxy port = findPort(request, zone);
        if (port.isQueued()) {
            size_t length = port.maxQueueLength();
            std::map<std::string, msgpack::object> result;
            result["type"] = msgpack::object("response", zone);
            std::map<std::string, msgpack::object> content;
            content["value"] = msgpack::object(length, zone);
            result["content"] = msgpack::object(content, zone);
            sendResponse(msgpack::object(result, zone), false);
        }
        else {
            sendErrorResponse("not a queued receiver port", zone);
        }
    }
    catch (std::runtime_error& e) {
        // error message has been sent by findPort
    }
}

void RemoteControl::setPortMaxQueueLength(
        const msgpack::object& request, msgpack::zone& zone) {
    try {
        mcf::PortProxy port = findPort(request, zone);
        auto map = request.as<std::map<std::string, msgpack::object>>();
        if (map.find("value") != map.end()) {
            size_t length = map["value"].as<size_t>();
            if (port.isQueued()) {
                port.setMaxQueueLength(length);
                sendEmptyResponse(zone);
            }
            else {
                sendErrorResponse("not a queued receiver port", zone);
            }
        }
        else {
            sendErrorResponse("no value given", zone);
        }
    }
    catch (std::runtime_error& e) {
        // error message has been sent by findPort
    }
}

void RemoteControl::setQueue(const msgpack::object& request, msgpack::zone& zone) {
    auto map = request.as<std::map<std::string, msgpack::object>>();

    if (map.find("topic") != map.end()) {
        auto topic = map["topic"].as<std::string>();

        if (map.find("size") != map.end()) {
            auto size = map["size"].as<int>();

            if (size <= 0) {
                // remove queue
                if (fQueueMap.find(topic) != fQueueMap.end()) {
                    fValueStore.removeReceiver(topic, fQueueMap[topic]);
                    fQueueMap.erase(topic);
                }
            }
            else {
                bool blocking;
                if (map.find("blocking") != map.end()) {
                    blocking = map["blocking"].as<bool>();
                }
                else {
                    blocking = false;
                }

                if (fQueueMap.find(topic) != fQueueMap.end()) {
                    fValueStore.removeReceiver(topic, fQueueMap[topic]);
                }
                fQueueMap[topic] = std::make_shared<mcf::ValueQueue>(size, blocking);
                fValueStore.addReceiver(topic, fQueueMap[topic]);
            }
            sendEmptyResponse(zone);
        }
        else {
            sendErrorResponse("no size given", zone);
        }
    }
    else {
        sendErrorResponse("no topic given", zone);
    }
}


void RemoteControl::readValue(const msgpack::object& request, msgpack::zone& zone) {
    auto map = request.as<std::map<std::string, msgpack::object>>();

    if (map.find("topic") != map.end()) {
        auto topic = map["topic"].as<std::string>();

        ValuePtr value = nullptr;
        if (fQueueMap.find(topic) != fQueueMap.end()) {
            auto queue = fQueueMap[topic];
            if (!queue->empty()) {
                value = queue->pop<Value>();

                auto typeInfoPtr = fValueStore.getTypeInfo(*value);
                if (typeInfoPtr != nullptr) {
                    sendResponseWithValue(zone, true, "has_more", !queue->empty());
                    remote::sendValue(value, *typeInfoPtr, fSocket, false);
                }
                else {
                    // could not serialize unknown type
                    sendResponseWithValue(zone, false, "has_more", !queue->empty());
                }
            }
            else {
                sendResponseWithValue(zone, false, "has_more", false);
            }
        }
        else {
            if (fValueStore.hasValue(topic)) {
                value = fValueStore.getValue<Value>(topic);
                auto typeInfoPtr = fValueStore.getTypeInfo(*value);
                if (typeInfoPtr != nullptr) {
                    sendEmptyResponse(zone, true);
                    remote::sendValue(value, *typeInfoPtr, fSocket, false);
                }
                else {
                    // could not serialize unknown type
                    sendEmptyResponse(zone, false);
                }
            }
            else {
                sendEmptyResponse(zone, false);
            }
        }
    }
    else {
        sendErrorResponse("no topic given", zone);
    }
}

void RemoteControl::processRequest(const msgpack::object& request) {
    msgpack::zone zone;
    std::map<std::string, msgpack::object> result;
    if (request.type == msgpack::type::MAP) {
        auto map = request.as<std::map<std::string, msgpack::object>>();
        if (map.find("command") != map.end()) {
            auto cmd = map.at("command").as<std::string>();
            if (cmd == "get_info") {
                result["type"] = msgpack::object("response", zone);
                result["content"] = commandGetInfo(request, zone);
                sendResponse(msgpack::object(result, zone));
            }
            else if (cmd == "connect_port") {
                connectPort(request, zone, true);
            }
            else if (cmd == "disconnect_port") {
                connectPort(request, zone, false);
            }
            else if (cmd == "read_value") {
                readValue(request, zone);
            }
            else if (cmd == "write_value") {
                writeValue(request, zone);
            }
            else if (cmd == "set_queue") {
                setQueue(request, zone);
            }
            else if (cmd == "event_queue_info")
            {
                result["type"] = msgpack::object("response", zone);
                result["content"] = commandEventQueueInfo(zone);
                sendResponse(msgpack::object(result, zone));
            }
            else if (cmd == "enable_event_queue")
            {
                commandEnableEventQueue(request, zone);
            }
            else if (cmd == "set_playback_modifier")
            {
                setPlaybackModifier(request, zone);
            }
            else if (cmd == "set_replay_params")
            {
                setReplayParams(request, zone);
            }
            else if (cmd == "get_replay_params")
            {
                getReplayParams(zone);
            }
            else if (cmd == "get_sim_time")
            {
                getSimTime(zone);
            }
            else if (cmd == "get_port_blocking") {
                getPortBlocking(request, zone);
            }
            else if (cmd == "set_port_blocking") {
                setPortBlocking(request, zone);
            }
            else if (cmd == "get_port_max_queue_length") {
                getPortMaxQueueLength(request, zone);
            }
            else if (cmd == "set_port_max_queue_length") {
                setPortMaxQueueLength(request, zone);
            }
            else {
                sendErrorResponse("unknown command: "+cmd, zone);
            }
        }
        else {
            sendErrorResponse("missing command", zone);
        }
    }
    else {
        sendErrorResponse("request is not a map", zone);
    }
}

void RemoteControl::commandEnableEventQueue(const msgpack::object& request, msgpack::zone& zone)
{
    auto map = request.as<std::map<std::string, msgpack::object>>();
    if (map.find("enabled") != map.end())
    {
        bool enabled = map["enabled"].as<bool>();
        if (!enableEventQueueing(enabled))
        {
            sendErrorResponse("Cannot enable event queueing. Remote control event source does not exist.", zone);
        }
        else
        {
            sendEmptyResponse(zone);
        }
    }
    else
    {
        sendErrorResponse("missing enabled flag value", zone);
    }
}

/*
 * Enable/disable event queuing
 */
bool RemoteControl::enableEventQueueing(bool enabled)
{
    MCF_ASSERT(fReplayEventController, "ReplayEventController must be passed to RemoteControl in order to use event queueing.");
    if (!fRemoteControlEventSource)
    {
        MCF_WARN_NOFILELINE("Cannot enable event queueing. Remote control event source does not exist.");
        return false;
    }

    if (enabled && !fIsEventQueueEnabled)
    {
        fReplayEventController->addEventSource(fRemoteControlEventSource, "remote_control");
    }
    else if (!enabled && fIsEventQueueEnabled)
    {
        fReplayEventController->removeEventSource("remote_control");
    }
    fIsEventQueueEnabled.store(enabled);

    return true;
}

void RemoteControl::createRemoteControlEventSource()
{
    MCF_ASSERT(fReplayEventController, "ReplayEventController must be passed to RemoteControl in order to creat event queue.");
    std::weak_ptr<mcf::IEventTimingController> eventTimingController = fReplayEventController->getEventTimingController();
    fRemoteControlEventSource = std::make_shared<QueuedEventSource>(fValueStore, eventTimingController);
}

msgpack::object RemoteControl::commandEventQueueInfo(msgpack::zone& zone)
{
    std::size_t queueSize = 0;
    QueuedEventSource::IntTimestamp firstTime = 0UL;
    QueuedEventSource::IntTimestamp lastTime = 0UL;

    if (fIsEventQueueEnabled.load())
    {
        fRemoteControlEventSource->getEventQueueInfo(queueSize, firstTime, lastTime);
    }

    std::map<std::string, msgpack::object> queueState;
    queueState["size"] = msgpack::object(queueSize, zone);
    queueState["enabled"] = msgpack::object(fIsEventQueueEnabled.load(), zone);
    queueState["first_time"] = msgpack::object(firstTime, zone);
    queueState["last_time"] = msgpack::object(lastTime, zone);

    return { queueState, zone };
}


void RemoteControl::writeValue(const msgpack::object& request, msgpack::zone& zone) {
    auto map = request.as<std::map<std::string, msgpack::object>>();

    if (map.find("topic") != map.end()) {
        auto topic = map["topic"].as<std::string>();

        auto timeEntry = map.find("timestamp");
        bool hasTime = (timeEntry != map.end());

        if (fSocket.getsockopt<int>(ZMQ_RCVMORE)) {
            ValuePtr value = nullptr;
            try {
                value = remote::receiveValue(fValueStore, fSocket);
            }
            catch (remote::ReceiveError& e) {
                std::string errorMsg = fmt::format(
                    "In RemoteControl receiveValue: receive error: {}, topic: {}",
                    e.what(),
                    topic);
                MCF_ERROR_NOFILELINE(errorMsg);
                sendErrorResponse(errorMsg, zone);
                return;
            }
            if (value != nullptr) {
                std::string component;
                if (map.find("component") != map.end())
                {
                    component = map["component"].as<std::string>();
                }

                std::string port;
                if (map.find("port") != map.end())
                {
                    port = map["port"].as<std::string>();
                }

                // event queue not enabled, or no timestamp is received, publish immediately
                if (!fIsEventQueueEnabled.load() || !hasTime)
                {
                    fValueStore.setValue(topic, value);
                }
                // otherwise (timestamp received and queue enabled), put value into queue
                else
                {
                    QueuedEventSource::IntTimestamp intTimestamp = (timeEntry->second).as<QueuedEventSource::IntTimestamp>();
                    mcf::TimestampType timestamp(intTimestamp);
                    fRemoteControlEventSource->pushNewEvent(timestamp, topic, value, component, port);
                }

                sendEmptyResponse(zone);
            }
            else {
                sendErrorResponse("receive error", zone);
            }
        }

        else {
            sendErrorResponse("no value message part", zone);
        }
    }
    else {
        sendErrorResponse("no topic given", zone);
    }
}

std::shared_ptr<QueuedEventSource> RemoteControl::getRemoteControlEventSource()
{
    return fRemoteControlEventSource;
}

void RemoteControl::setReplayEventController(std::shared_ptr<ReplayEventController> replayEventController)
{
    // Ensure that the replay event controller has not already been set.
    MCF_ASSERT(!fReplayEventController, "ReplayEventController has already been set.");
    fReplayEventController = replayEventController;
}

void RemoteControl::setPlaybackModifier(const msgpack::object& request, msgpack::zone& zone)
{
    if(!fReplayEventController)
    {
        sendErrorResponse("no ReplayEventController", zone);
        return;
    }

    ReplayEventController::PlaybackModifier playbackModifier;

    auto map = request.as<std::map<std::string, msgpack::object>>();

    if (map.find("playback_modifier") != map.end()) {
        playbackModifier = static_cast<ReplayEventController::PlaybackModifier>(map["playback_modifier"].as<unsigned int>());
        fReplayEventController->setPlaybackModifier(playbackModifier);
        sendEmptyResponse(zone);
    }
    else {
        sendErrorResponse("no playback modifier given", zone);
    }
}

void RemoteControl::setReplayParams(const msgpack::object& request, msgpack::zone& zone)
{
    if(!fReplayEventController)
    {
        sendErrorResponse("no ReplayEventController", zone);
        return;
    }

    ReplayEventController::Params params;

    auto map = request.as<std::map<std::string, msgpack::object>>();

    if (map.find("run_mode") != map.end()) {
        params.runMode = static_cast<ReplayEventController::RunMode>(map["run_mode"].as<int>());

        if (map.find("run_without_drops") != map.end()) {
            params.runWithoutDrops = map["run_without_drops"].as<bool>();

            if (map.find("speed_factor") != map.end()) {
                params.speedFactor = map["speed_factor"].as<float>();

                if (map.find("pipeline_end_trigger_names") != map.end()) {
                    params.pipelineEndTriggerNames = map["pipeline_end_trigger_names"].as<std::vector<std::string> >();

                    if (map.find("wait_input_event_name") != map.end()) {
                        params.waitInputEventName = map["wait_input_event_name"].as<std::string>();

                        if (map.find("wait_input_event_topic") != map.end()) {
                            params.waitInputTopicName = map["wait_input_event_topic"].as<std::string>();

                            if (map.find("step_time_microseconds") != map.end()) {
                                params.stepTimeMicroSeconds = map["step_time_microseconds"].as<uint64_t>();

                                fReplayEventController->setParams(params);
                                sendEmptyResponse(zone);
                            }
                            else {
                                sendErrorResponse("no step time given", zone);
                            }
                        }
                        else {
                            sendErrorResponse("no wait input event topic given", zone);
                        }
                    }
                    else {
                        sendErrorResponse("no wait input event name given", zone);
                    }
                }
                else {
                    sendErrorResponse("no pipeline end trigger names given", zone);
                }
            }
            else {
                sendErrorResponse("no speed factor given", zone);
            }
        }
        else {
            sendErrorResponse("no run without drops flag given", zone);
        }
    }
    else {
        sendErrorResponse("no run mode given", zone);
    }
}

void RemoteControl::getReplayParams(msgpack::zone& zone)
{
    if(!fReplayEventController)
    {
        sendErrorResponse("no ReplayEventController", zone);
        return;
    }

    const auto params = fReplayEventController->getParams();

    std::map<std::string, msgpack::object> replayParams;
    replayParams["run_mode"] = msgpack::object(static_cast<int>(params.runMode), zone);
    replayParams["run_without_drops"] = msgpack::object(params.runWithoutDrops, zone);
    replayParams["speed_factor"] = msgpack::object(params.speedFactor, zone);
    replayParams["pipeline_end_trigger_names"] = msgpack::object(params.pipelineEndTriggerNames, zone);
    replayParams["wait_input_event_name"] = msgpack::object(params.waitInputEventName, zone);
    replayParams["wait_input_event_topic"] = msgpack::object(params.waitInputTopicName, zone);
    replayParams["step_time_microseconds"] = msgpack::object(params.stepTimeMicroSeconds, zone);

    std::map<std::string, msgpack::object> result;
    result["type"] = msgpack::object("response", zone);
    result["content"] = msgpack::object(replayParams, zone);
    sendResponse(msgpack::object(result, zone));
}


void RemoteControl::getSimTime(msgpack::zone& zone)
{
    if(!fReplayEventController)
    {
        sendErrorResponse("no ReplayEventController", zone);
        return;
    }

    uint64_t simTimeInt = 0;
    TimestampType simulationTime;

    bool isInitialised = fReplayEventController->getTime(simulationTime);
    if (isInitialised)
    {
        simTimeInt = static_cast<uint64_t>(simulationTime);
    }

    std::map<std::string, msgpack::object> simTimeMsgPack;
    simTimeMsgPack["is_initialised"] = msgpack::object(isInitialised, zone);
    simTimeMsgPack["sim_time"] = msgpack::object(simTimeInt, zone);

    std::map<std::string, msgpack::object> result;
    result["type"] = msgpack::object("response", zone);
    result["content"] = msgpack::object(simTimeMsgPack, zone);
    sendResponse(msgpack::object(result, zone));
}

} // end namespace remote

} // end namespace mcf


