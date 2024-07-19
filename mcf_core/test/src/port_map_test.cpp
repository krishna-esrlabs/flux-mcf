/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_core/Mcf.h"
#include "test/TestUtils.h"
#include "test/TestValue.h"

namespace mcf
{
class PortMapTest : public ::testing::Test
{
public:
    using TestValue = mcf::test::TestValue;
    class TestValueExtMem;

    class TestComponent : public mcf::Component
    {
    public:
        TestComponent()
        : mcf::Component("TestComponent"), fTickPort(*this, "tick"), fTackPort(*this, "tack")
        {
            fTickPort.registerHandler(std::bind(&TestComponent::tick, this));
        }

        void configure(mcf::IComponentConfig& config)
        {
            config.registerPort(fTickPort);
            config.registerPort(fTackPort);
        }

        void tick()
        {
            int val = fTickPort.getValue()->val;
            events.push_back(std::string("/tick:") + std::to_string(val));
            fTackPort.setValue(TestValue(val));
        }

        std::vector<std::string> events;
        mcf::ReceiverPort<TestValue> fTickPort;
        mcf::SenderPort<TestValue> fTackPort;
    };

    class Consumer : public mcf::Component
    {
    public:
        Consumer() : mcf::Component("Consumer"), fConsumerPort(*this, "Consumer", 1, true) {}

        void configure(mcf::IComponentConfig& config)
        {
            config.registerPort(fConsumerPort, "/listener");
        }

        int consume() { return fConsumerPort.getValue()->val; }

        mcf::QueuedReceiverPort<TestValue> fConsumerPort;
    };
};

TEST_F(PortMapTest, MapPortsAtRuntime)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto component = std::make_shared<TestComponent>();

    auto proxy = manager.registerComponent(component);

    proxy.configure();

    proxy.startup();
    proxy.mapPort("tick", "/tick");
    proxy.mapPort("tack", "/tack");
    proxy.port("tick").connect();
    proxy.port("tack").connect();

    valueStore.setValue("/tick", TestValue(17));

    waitForValue(valueStore, "/tack");
    EXPECT_TRUE(valueStore.hasValue("/tack"));
    EXPECT_EQ(valueStore.getValue<TestValue>("/tack")->val, 17);
}

TEST_F(PortMapTest, MapPortsAfterConfigure)
{
    /*
     * Test the ability to map ports directly after instantiation/registration/configuration
     */
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto component = std::make_shared<TestComponent>();

    auto proxy = manager.registerComponent(component);

    proxy.configure();
    proxy.mapPort("tick", "/tick");
    proxy.mapPort("tack", "/tack");

    proxy.startup();
    valueStore.setValue("/tick", TestValue(17));

    waitForValue(valueStore, "/tack");
    EXPECT_TRUE(valueStore.hasValue("/tack"));
    EXPECT_EQ(valueStore.getValue<TestValue>("/tack")->val, 17);
}

TEST_F(PortMapTest, DynamicRemapping)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto component = std::make_shared<TestComponent>();
    auto consumer  = std::make_shared<Consumer>();

    auto proxy         = manager.registerComponent(component);
    auto consumerProxy = manager.registerComponent(consumer);

    manager.configure();
    manager.startup();

    proxy.mapPort("tick", "/tick");
    proxy.mapPort("tack", "/tack");
    proxy.port("tick").connect();
    proxy.port("tack").connect();

    valueStore.setValue("/tick", TestValue(17));

    waitForValue(valueStore, "/tack");
    EXPECT_TRUE(valueStore.hasValue("/tack"));
    EXPECT_EQ(valueStore.getValue<TestValue>("/tack")->val, 17);

    proxy.mapPort("tack", "/anotherTack");
    valueStore.setValue("/tick", TestValue(42));
    waitForValue(valueStore, "/anotherTack");

    EXPECT_TRUE(valueStore.hasValue("/anotherTack"));
    EXPECT_EQ(valueStore.getValue<TestValue>("/anotherTack")->val, 42);
    EXPECT_FALSE(
        valueStore.hasValue("/tack") && valueStore.getValue<TestValue>("/tack")->val == 42);
}

TEST_F(PortMapTest, Interface)
{
    /*
     * Test the ability to map ports from different interfaces
     */
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto component = std::make_shared<TestComponent>();

    auto proxy = manager.registerComponent(component);

    proxy.configure();

    // Use a PortProxy object
    auto ports = proxy.ports();
    for (auto& port : ports)
    {
        if (port.name() == "tick")
        {
            port.mapToTopic("/tick");
        }
    }

    // Use ComponentManager
    manager.mapPort(proxy, "tack", "/tack");

    proxy.startup();
    valueStore.setValue("/tick", TestValue(17));

    waitForValue(valueStore, "/tack");
    EXPECT_TRUE(valueStore.hasValue("/tack"));
    EXPECT_EQ(valueStore.getValue<TestValue>("/tack")->val, 17);
}

} // namespace mcf