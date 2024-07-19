/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_core/ComponentInstantiator.h"
#include "mcf_core/ExtMemValue.h"
#include "mcf_core/Mcf.h"
#include "test/TestUtils.h"
#include "test/TestValue.h"

namespace mcf
{
class InstantiatorTest : public ::testing::Test
{
public:
    using TestValue = mcf::test::TestValue;
    class TestValueExtMem;

    class TestComponent : public mcf::Component
    {
    public:
        TestComponent()
        : mcf::Component("TestComponent"), fTickPort(*this, "Tick"), fTackPort(*this, "Tack")
        {
            fTickPort.registerHandler(std::bind(&TestComponent::tick, this));
        }

        void configure(mcf::IComponentConfig& config)
        {
            config.registerPort(fTickPort, "/tick");
            config.registerPort(fTackPort, "/tack");
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

    class CounterReceiver : public mcf::Component
    {
    public:
        CounterReceiver()
        : mcf::Component("CounterReceiver"), fConsumerPort(*this, "Consumer", 1, true)
        {
        }

        void configure(mcf::IComponentConfig& config)
        {
            config.registerPort(fConsumerPort, "/counter");
        }

        int consume() { return fConsumerPort.getValue()->val; }

        mcf::QueuedReceiverPort<TestValue> fConsumerPort;
    };

    class TestComponent3 : public mcf::Component
    {
    public:
        TestComponent3()
        : mcf::Component("TestComponent3")
        , fCounterPort(*this, "Counter")
        , fTriggerPort(*this, "Trigger")
        , fCounter(0)
        {
        }

        void configure(mcf::IComponentConfig& config)
        {
            config.registerPort(fCounterPort, "/counter");
            config.registerPort(fTriggerPort, "/trigger");
            fCounter.store(1);
            fTriggerPort.registerHandler(std::bind(&TestComponent3::trigger, this));
        }

        void trigger() { fCounterPort.setValue(TestValue(fCounter.fetch_add(1))); }

        mcf::SenderPort<TestValue> fCounterPort;
        mcf::ReceiverPort<TestValue> fTriggerPort;
        std::atomic<uint64_t> fCounter;
    };

    template <typename T>
    void waitForQueue(mcf::QueuedReceiverPort<T>& port)
    {
        for (std::size_t i = 0; i < 10; ++i)
        {
            if (port.getQueueSize() == 1)
            {
                break;
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
};

TEST_F(InstantiatorTest, SimpleInstantiation)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);

    instantiator.addComponentType(ComponentType::create<TestComponent>("esrlabs/Test"));

    auto loadedTypes = instantiator.listComponentTypes();
    EXPECT_EQ(loadedTypes, std::vector<std::string>({"esrlabs/Test"}));

    instantiator.createComponent("esrlabs/Test", "test1");

    manager.configure();
    manager.startup();

    EXPECT_EQ(manager.getComponents().at(0).name(), "test1");

    valueStore.setValue("/tick", TestValue(23));
    waitForValue(valueStore, "/tack");
    EXPECT_TRUE(valueStore.hasValue("/tack"));
    auto ptr = valueStore.getValue<TestValue>("/tack");
    EXPECT_EQ(23, ptr->val);

    manager.shutdown();
}

TEST_F(InstantiatorTest, InstantiateAndCleanup)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);

    instantiator.addComponentType(ComponentType::create<TestComponent>("esrlabs/Test"));

    auto loadedTypes = instantiator.listComponentTypes();
    EXPECT_EQ(loadedTypes, std::vector<std::string>({"esrlabs/Test"}));

    instantiator.createComponent("esrlabs/Test", "test1");

    manager.configure();
    manager.startup();

    valueStore.setValue("/tick", TestValue(23));

    waitForValue(valueStore, "/tack");
    EXPECT_TRUE(valueStore.hasValue("/tack"));
    auto ptr = valueStore.getValue<TestValue>("/tack");
    EXPECT_EQ(23, ptr->val);

    instantiator.removeComponent("test1");

    EXPECT_EQ(0, instantiator.listComponents().size());
    EXPECT_EQ(0, manager.getComponents().size());

    valueStore.setValue("/tick", TestValue(42));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    ptr = valueStore.getValue<TestValue>("/tack");
    // expect old value
    EXPECT_EQ(23, ptr->val);

    manager.shutdown();
}

TEST_F(InstantiatorTest, Restart)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);

    instantiator.addComponentType(ComponentType::create<TestComponent3>("esrlabs/Test"));

    auto proxy    = instantiator.createComponent("esrlabs/Test", "test1");
    auto consumer = std::make_shared<CounterReceiver>();
    manager.registerComponent(consumer);

    manager.configure();
    manager.startup();

    valueStore.setValue("/trigger", TestValue(0));
    waitForValue(valueStore, "/counter");

    waitForQueue(consumer->fConsumerPort);

    EXPECT_TRUE(valueStore.hasValue("/counter"));
    EXPECT_TRUE(consumer->fConsumerPort.getBlocking());
    EXPECT_EQ(consumer->fConsumerPort.getQueueSize(), 1);
    EXPECT_EQ(consumer->fConsumerPort.getMaxQueueLength(), 1);
    auto ptr = valueStore.getValue<TestValue>("/counter");
    EXPECT_EQ(1, ptr->val);

    EXPECT_EQ(1, consumer->consume());

    valueStore.setValue("/trigger", TestValue(0));

    waitForQueue(consumer->fConsumerPort);

    ptr = valueStore.getValue<TestValue>("/counter");
    EXPECT_EQ(2, ptr->val);
    EXPECT_EQ(2, consumer->consume());

    auto proxy2 = instantiator.reloadComponent("test1");
    proxy2.configure();
    proxy2.startup();

    valueStore.setValue("/trigger", TestValue(0));

    waitForQueue(consumer->fConsumerPort);

    ptr = valueStore.getValue<TestValue>("/counter");
    EXPECT_EQ(1, ptr->val);

    manager.shutdown();
}

TEST_F(InstantiatorTest, Exceptions)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);

    instantiator.addComponentType(ComponentType::create<TestComponent>("esrlabs/Test"));
    EXPECT_THROW(
        instantiator.addComponentType(ComponentType::create<TestComponent>("esrlabs/Test")),
        mcf::ComponentInstantiationError);

    auto loadedTypes = instantiator.listComponentTypes();
    EXPECT_EQ(loadedTypes, std::vector<std::string>({"esrlabs/Test"}));

    instantiator.createComponent("esrlabs/Test", "test1");
    EXPECT_THROW(
        instantiator.createComponent("esrlabs/Test", "test1"), mcf::ComponentInstantiationError);
    EXPECT_THROW(
        instantiator.createComponent("esrlabs/NoSuchComponent", "test"),
        mcf::ComponentInstantiationError);
    EXPECT_THROW(instantiator.removeComponent("test3"), mcf::ComponentInstantiationError);
}

} // namespace mcf