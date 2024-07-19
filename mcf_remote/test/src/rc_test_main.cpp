/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_remote/RemoteControl.h"
#include "mcf_core/ReplayEventController.h"
#include "mcf_core/EventTimingController.h"
#include "mcf_core/QueuedEventSource.h"
#include "mcf_core/Component.h"
#include "mcf_core/ISimpleEventSource.h"
#include "mcf_remote_test_value_types/McfRemoteTestValueTypes.h"

#include "CLI/CLI.hpp"

#include <chrono>
#include <thread>
#include <iostream>


// Create a dummy event every 1ms so as to keep EventTimingController "alive"
class DummyEventSource : public mcf::ISimpleEventSource
{
public:

    using Microseconds = std::chrono::microseconds;

    DummyEventSource() = default;

    explicit DummyEventSource(mcf::TimestampType startTime)
    : fNextEventTime(startTime)
    {}

    explicit DummyEventSource(uint64_t microSecsSinceEpoch)
    : fNextEventTime(Microseconds(microSecsSinceEpoch))
    {}

    bool getNextEventTime(mcf::TimestampType &nextEventTimestamp) override
    {
        nextEventTimestamp = fNextEventTime;
        return true;
    }

    bool fireEvent() override
    {
        fNextEventTime = static_cast<mcf::TimestampType>(
            fNextEventTime + static_cast<mcf::TimestampType>(Microseconds(1000UL)));
        return true;
    }

private:

    mcf::TimestampType fNextEventTime = static_cast<mcf::TimestampType>(Microseconds(1UL));
};

class ControlValue : public mcf::Value {
public:
    ControlValue() : cmd("") {};
    ControlValue(std::string cmd) : cmd(cmd) {};
    std::string cmd;
    MSGPACK_DEFINE(cmd);
};

template<typename T>
static inline void registerValueTypes(T& r) {
    r.template registerType<ControlValue>("TCControlValue");
}

// Component sending bursts of 3 points when receiving the corresponding command value
class TestComponent1 : public mcf::Component {

public:
    TestComponent1() :
        Component("TestComponent1"),
        fPointSender(*this, "PointSender"),
        fPointReceiver(*this, "PointReceiver"),
        fControlPort(*this, "ControlPort", 0)
    {
        fPointReceiver.registerHandler(std::bind(&TestComponent1::pointIn, this));
        fControlPort.registerHandler(std::bind(&TestComponent1::control, this));
    }

    void configure(mcf::IComponentConfig& config) {
        config.registerPort(fPointSender, "/points_connected");
        config.registerPort(fPointReceiver, "/points_tc1_in");
        config.registerPort(fControlPort, "/tc1_control");
    }

    void control() {
        auto cmd = fControlPort.getValue();
        if (cmd->cmd == "sendBurst") {
            fPointSender.setValue(values::mcf_remote_test_value_types::mcf_remote_test::TestPointXY(1, 1));
            fPointSender.setValue(values::mcf_remote_test_value_types::mcf_remote_test::TestPointXY(2, 2));
            fPointSender.setValue(values::mcf_remote_test_value_types::mcf_remote_test::TestPointXY(3, 3));
        }
    }

    void pointIn() {
        fPointSender.setValue(fPointReceiver.getValue());
    }

    mcf::SenderPort<values::mcf_remote_test_value_types::mcf_remote_test::TestPointXY> fPointSender;
    mcf::ReceiverPort<values::mcf_remote_test_value_types::mcf_remote_test::TestPointXY> fPointReceiver;
    mcf::QueuedReceiverPort<ControlValue> fControlPort;
};

// Component consuming/forwarding points, depending on the value on the received command
class TestComponent2 : public mcf::Component {
public:
    TestComponent2() :
        Component("TestComponent2"),
        fForwardEnabled(true),
        fPointSender(*this, "PointSender"),
        fPointReceiver(*this, "PointReceiver", 3, true),
        fControlPort(*this, "ControlPort", 0)
    {
        fPointReceiver.registerHandler(std::bind(&TestComponent2::pointIn, this));
        fControlPort.registerHandler(std::bind(&TestComponent2::control, this));
    }

    void configure(mcf::IComponentConfig& config) {
        config.registerPort(fPointReceiver, "/points_connected");
        config.registerPort(fPointSender, "/points_tc2_out");
        config.registerPort(fControlPort, "/tc2_control");
    }

    void control() {
        auto cmd = fControlPort.getValue();
        if (cmd->cmd == "stopForwarding") {
            fForwardEnabled = false;
        }
        else if (cmd->cmd == "startForwarding") {
            fForwardEnabled = true;
        }
        else if (cmd->cmd == "consumePoint") {
            fPointReceiver.getValue();
        }
    }

    void pointIn() {
        if (fForwardEnabled) {
            fPointSender.setValue(fPointReceiver.getValue());
        }
    }

    bool fForwardEnabled;
    mcf::SenderPort<values::mcf_remote_test_value_types::mcf_remote_test::TestPointXY> fPointSender;
    mcf::QueuedReceiverPort<values::mcf_remote_test_value_types::mcf_remote_test::TestPointXY> fPointReceiver;
    mcf::QueuedReceiverPort<ControlValue> fControlPort;
};

int main(int argc, char **argv) {

    CLI::App app("MCF Remote Control Test Engine");

    std::string configDirectory = "./";
    app.add_option("--config-dir", configDirectory,
            "Full directory path containing component configuration files");

    CLI11_PARSE(app, argc, argv)

    mcf::ValueStore valueStore;
    values::mcf_remote_test_value_types::registerMcfRemoteTestValueTypes(valueStore);
    registerValueTypes(valueStore);

    mcf::ComponentManager componentManager(valueStore, configDirectory);

    // create and register test components
    auto tc1 = std::make_shared<TestComponent1>();
    auto tc2 = std::make_shared<TestComponent2>();
    componentManager.registerComponent(tc1);
    componentManager.registerComponent(tc2);

    // create and register remote control
    auto remoteControl = std::make_shared<mcf::remote::RemoteControl>(
        6666,
        componentManager,
        valueStore);
    auto replayEventController = std::make_shared<mcf::ReplayEventController>(valueStore);
    remoteControl->setReplayEventController(replayEventController);
    remoteControl->createRemoteControlEventSource();
    remoteControl->enableEventQueueing(true);
    auto rcEvents = remoteControl->getRemoteControlEventSource();

    // create and register the dummy event source
    auto dummyEvents = std::make_shared<DummyEventSource>();
    replayEventController->addEventSource(dummyEvents, "dummy");
    componentManager.registerComponent(remoteControl);

    componentManager.configure();
    componentManager.startup();

    // wait until first python event has arrived
    mcf::TimestampType dummyTimestamp;
    std::string dummyTopic;
    while (!rcEvents->getNextEventInfo(dummyTimestamp, dummyTopic))
    {
    }

    // Playback runs until user interrupts playback or there are no more events
    replayEventController->run();
    while (!replayEventController->isFinished())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    componentManager.shutdown();

    return 0;
}
