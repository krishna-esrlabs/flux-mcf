/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_core/Mcf.h"

namespace mcf {

class LogTest : public ::testing::Test {
public:
    class TestValue;

    class TestComponent : public Component {

    public:
        TestComponent() :
            Component("TestComponent"),
            fActionPort(*this, "Trigger"),
            fSelectorPort(*this, "Selector")
        {
            fActionPort.registerHandler(std::bind(&TestComponent::action, this));
            fSelectorPort.registerHandler([this] () { logSpecificValue(); });
        }

        void configure(IComponentConfig& config) {
            config.registerPort(fActionPort, "/logSomething");
            config.registerPort(fSelectorPort, "/logSelectively");
        }

        void action() {
            log(LogSeverity::trace, "trace");
            log(LogSeverity::debug, "debug");
            log(LogSeverity::info, "info");
            log(LogSeverity::warn, "warn");
            log(LogSeverity::err, "error");
            log(LogSeverity::critical, "fatal");
        }

        void logSpecificValue() {
            auto logSeverity = fSelectorPort.getValue();
            if (logSeverity->val >= LogSeverity::trace && logSeverity->val <= LogSeverity::critical) {
                log(static_cast<LogSeverity>(logSeverity->val), "message");
            }
        }

        ReceiverPort<TestValue> fActionPort;
        ReceiverPort<TestValue> fSelectorPort;
    };

    class TestValue : public mcf::Value {
    public:
        TestValue(int val=0) : val(val) {};
        int val;
        MSGPACK_DEFINE(val);
    };
};

TEST_F(LogTest, DefaultLevels) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto testComponent = std::make_shared<LogTest::TestComponent>();

    manager.registerComponent(testComponent);

    manager.configure();
    manager.startup();

    auto logMessages = std::make_shared<mcf::ValueQueue>();
    valueStore.addReceiver("/mcf/log/TestComponent/message", logMessages);

    valueStore.setValue("/logSomething", TestValue(1));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(!logMessages->empty());

    auto msg = logMessages->pop<mcf::msg::LogMessage>();
    EXPECT_EQ(3, msg->severity);
    EXPECT_EQ("error", msg->message);
    ASSERT_TRUE(!logMessages->empty());

    msg = logMessages->pop<mcf::msg::LogMessage>();
    EXPECT_EQ(4, msg->severity);
    EXPECT_EQ("fatal", msg->message);
    ASSERT_TRUE(logMessages->empty());

    manager.shutdown();
}

TEST_F(LogTest, SetLogLevelMin) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto testComponent = std::make_shared<LogTest::TestComponent>();

    manager.registerComponent(testComponent);

    manager.configure();
    manager.startup();

    auto logMessages = std::make_shared<mcf::ValueQueue>();
    valueStore.addReceiver("/mcf/log/TestComponent/message", logMessages);

    mcf::msg::LogControl logControl;
    logControl.level = 0;

    valueStore.setValue("/mcf/log/TestComponent/control", logControl);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    valueStore.setValue("/logSomething", TestValue(1));
    // wait until there are messages
    for (int tries = 0; tries < 10 && logMessages->empty(); ++tries)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(!logMessages->empty());

    auto msg = logMessages->pop<mcf::msg::LogMessage>();
    EXPECT_EQ(0, msg->severity);
    EXPECT_EQ("trace", msg->message);

    manager.shutdown();
}

TEST_F(LogTest, SetLogLevelMax) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto testComponent = std::make_shared<LogTest::TestComponent>();

    manager.registerComponent(testComponent);

    manager.configure();
    manager.startup();

    auto logMessages = std::make_shared<mcf::ValueQueue>();
    valueStore.addReceiver("/mcf/log/TestComponent/message", logMessages);

    mcf::msg::LogControl logControl;
    logControl.level = 5;

    valueStore.setValue("/mcf/log/TestComponent/control", logControl);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    valueStore.setValue("/logSomething", TestValue(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(logMessages->empty());

    manager.shutdown();
}

TEST_F(LogTest, LogMacros) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto testComponent = std::make_shared<LogTest::TestComponent>();

    manager.registerComponent(testComponent);
    manager.configure();
    manager.startup();

    std::thread([testComponent, &valueStore]() {
        auto logMessages = std::make_shared<mcf::ValueQueue>();
        valueStore.addReceiver("/mcf/log/TestComponent/message", logMessages);

        mcf::msg::LogControl logControl;
        logControl.level = 4;

        testComponent->injectLocalLogger();

        valueStore.setValue("/mcf/log/TestComponent/control", logControl);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        MCF_INFO("Info");

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_TRUE(logMessages->empty());

        MCF_FATAL("Fatal");

        ASSERT_TRUE(!logMessages->empty());

        auto msg = logMessages->pop<mcf::msg::LogMessage>();
        EXPECT_EQ(4, msg->severity);
        EXPECT_EQ("Fatal", msg->message);
    }).join();

    manager.shutdown();
}

TEST_F(LogTest, SeverityChange) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto testComponent = std::make_shared<LogTest::TestComponent>();
    manager.registerComponent(testComponent);
    manager.configure();
    manager.startup();

    auto logMessages = std::make_shared<mcf::ValueQueue>();
    valueStore.addReceiver("/mcf/log/TestComponent/message", logMessages);

    for (int i = LogSeverity::trace; i < LogSeverity::off; ++i)
    {
        const LogSeverity severity = static_cast<LogSeverity>(i);
        testComponent->ctrlSetLogLevels(severity, severity);
        valueStore.setValue("/logSelectively", TestValue(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_FALSE(logMessages->empty());
        if (!logMessages->empty())
        {
            logMessages->pop<mcf::msg::LogMessage>();
        }
    }

    manager.shutdown();
}
}

