/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_core/Mcf.h"
#include "test/TestUtils.h"
#include "test/TestValue.h"

namespace mcf
{
class SystemConfigurationTest : public ::testing::Test
{
public:
    using TestValue = mcf::test::TestValue;

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

        void configure(mcf::IComponentConfig& config) { config.registerPort(fConsumerPort); }

        int consume() { return fConsumerPort.getValue()->val; }

        mcf::QueuedReceiverPort<TestValue> fConsumerPort;
    };

    class StringConsumer : public mcf::Component
    {
    public:
        StringConsumer() : mcf::Component("StringConsumer"), fConsumerPort(*this, "Consumer") {}

        void configure(mcf::IComponentConfig& config) { config.registerPort(fConsumerPort); }

        mcf::ReceiverPort<mcf::msg::String> fConsumerPort;
    };

    class Producer : public mcf::Component
    {
    public:
        Producer()
        : mcf::Component("Producer")
        , fCounterPort(*this, "Counter")
        , fTriggerPort(*this, "Trigger")
        , fCounter(0)
        {
        }

        void configure(mcf::IComponentConfig& config)
        {
            config.registerPort(fCounterPort);
            config.registerPort(fTriggerPort);
            fCounter.store(1);
            fTriggerPort.registerHandler(std::bind(&Producer::trigger, this));
        }

        void trigger() { fCounterPort.setValue(TestValue(fCounter.fetch_add(1))); }

        mcf::SenderPort<TestValue> fCounterPort;
        mcf::ReceiverPort<TestValue> fTriggerPort;
        std::atomic<uint64_t> fCounter;
    };

    class SchedulableComponent : public mcf::Component
    {
    public:
        SchedulableComponent()
        : mcf::Component("SchedulableComponent")
        , fCounterPort(*this, "Counter")
        , fTriggerPort(*this, "Trigger")
        , fSchedulingParametersCounter(0)
        {
        }

        void configure(mcf::IComponentConfig& config)
        {
            config.registerPort(fCounterPort);
            config.registerPort(fTriggerPort);

            fTriggerPort.registerHandler(std::bind(&SchedulableComponent::tick, this));
        }

        void tick() { fCounterPort.setValue(TestValue(fSchedulingParametersCounter)); }

        virtual void ctrlSetSchedulingParameters(const SchedulingParameters& parameters) override
        {
            Component::ctrlSetSchedulingParameters(parameters);
            fSchedulingParametersCounter += 1;
        }

        std::atomic<uint64_t> fSchedulingParametersCounter;
        mcf::SenderPort<TestValue> fCounterPort;
        mcf::ReceiverPort<TestValue> fTriggerPort;
    };
};

TEST_F(SystemConfigurationTest, Initialize)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);
    mcf::ComponentSystemConfigurator configurator(manager, instantiator);

    instantiator.addComponentType(ComponentType::create<Producer>("esr/Producer"));
    instantiator.addComponentType(ComponentType::create<Consumer>("esr/Consumer"));
    instantiator.addComponentType(ComponentType::create<TestComponent>("esr/TestComponent"));

    auto configuration
        = system_configuration::ComponentSystem({system_configuration::ComponentInstance{
            "test",
            "esr/TestComponent",
            IComponent::SchedulingParameters{IComponent::SchedulingPolicy::Default, 0},
            std::vector<system_configuration::PortMapping>(
                {system_configuration::PortMapping{"tick", "/tick"},
                 system_configuration::PortMapping{"tack", "/tack"}})}});
    configurator.configure(configuration);

    EXPECT_EQ(manager.getComponents().size(), 1);

    manager.startup();

    valueStore.setValue("/tick", TestValue(17));

    waitForValue(valueStore, "/tack");
    EXPECT_TRUE(valueStore.hasValue("/tack"));
    EXPECT_EQ(valueStore.getValue<TestValue>("/tack")->val, 17);
}

TEST_F(SystemConfigurationTest, DynamicRemapping)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);
    mcf::ComponentSystemConfigurator configurator(manager, instantiator);

    instantiator.addComponentType(ComponentType::create<Producer>("esr/Producer"));
    instantiator.addComponentType(ComponentType::create<Consumer>("esr/Consumer"));
    instantiator.addComponentType(ComponentType::create<TestComponent>("esr/TestComponent"));

    auto configuration
        = system_configuration::ComponentSystem({system_configuration::ComponentInstance{
            "test",
            "esr/TestComponent",
            IComponent::SchedulingParameters{IComponent::SchedulingPolicy::Default, 0},
            std::vector<system_configuration::PortMapping>(
                {system_configuration::PortMapping{"tick", "/tick"},
                 system_configuration::PortMapping{"tack", "/tack"}})}});
    configurator.configure(configuration);

    EXPECT_EQ(manager.getComponents().size(), 1);
    EXPECT_EQ(manager.getComponents().at(0).name(), "test");

    manager.startup();

    valueStore.setValue("/tick", TestValue(17));

    waitForValue(valueStore, "/tack");
    EXPECT_TRUE(valueStore.hasValue("/tack"));
    EXPECT_EQ(valueStore.getValue<TestValue>("/tack")->val, 17);

    configurator.configure(
        system_configuration::ComponentSystem({system_configuration::ComponentInstance{
            "test",
            "",
            IComponent::SchedulingParameters{IComponent::SchedulingPolicy::Default, 0},
            std::vector<system_configuration::PortMapping>(
                {system_configuration::PortMapping{"tack", "/anotherTack"}})}}));

    valueStore.setValue("/tick", TestValue(42));
    waitForValue(valueStore, "/anotherTack");

    EXPECT_TRUE(valueStore.hasValue("/anotherTack"));
    EXPECT_EQ(valueStore.getValue<TestValue>("/anotherTack")->val, 42);
}

TEST_F(SystemConfigurationTest, Errors)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);
    mcf::ComponentSystemConfigurator configurator(manager, instantiator);

    instantiator.addComponentType(ComponentType::create<Producer>("esr/Producer"));
    instantiator.addComponentType(ComponentType::create<Consumer>("esr/Consumer"));
    instantiator.addComponentType(ComponentType::create<StringConsumer>("esr/StringConsumer"));
    instantiator.addComponentType(ComponentType::create<TestComponent>("esr/TestComponent"));

    std::string invalidConfiguration
        = "{\"ComponentSystemConfiguration\": { \"Components\": {\"test\": {\"type\": "
          "\"esr/TestComponent\", "
          "\"portMapping\": { \"tick\": \"/tick\", \"tack\": \"/tack\" } } } }"; // note the JSON is
                                                                                 // not valid
    EXPECT_THROW(configurator.configureFromJSON(invalidConfiguration), SystemConfigurationError);

    auto emptyType = system_configuration::ComponentSystem({system_configuration::ComponentInstance{
        "test",
        "",
        IComponent::SchedulingParameters{IComponent::SchedulingPolicy::Default, 0},
        std::vector<system_configuration::PortMapping>(
            {system_configuration::PortMapping{"tick", "/tick"},
             system_configuration::PortMapping{"tack", "/tack"}})}});
    EXPECT_THROW(configurator.configure(emptyType), SystemConfigurationError);
    EXPECT_EQ(manager.getComponents().size(), 0);

    auto emptyInstance
        = system_configuration::ComponentSystem({system_configuration::ComponentInstance{
            "",
            "esr/TestComponent",
            IComponent::SchedulingParameters{IComponent::SchedulingPolicy::Default, 0},
            std::vector<system_configuration::PortMapping>(
                {system_configuration::PortMapping{"tick", "/tick"},
                 system_configuration::PortMapping{"tack", "/tack"}})}});
    EXPECT_THROW(configurator.configure(emptyInstance), SystemConfigurationError);
    EXPECT_EQ(manager.getComponents().size(), 0);

    // partially correct configuration
    // test for atomicity
    auto partialConfiguration = system_configuration::ComponentSystem(
        {system_configuration::ComponentInstance{
             "test",
             "esr/TestComponent",
             IComponent::SchedulingParameters{IComponent::SchedulingPolicy::Default, 0},
             std::vector<system_configuration::PortMapping>(
                 {system_configuration::PortMapping{"tick", "/tick"},
                  system_configuration::PortMapping{"tack", "/tack"}})},
         system_configuration::ComponentInstance{
             "test",
             "esr/NoSuchComponentType",
             IComponent::SchedulingParameters{IComponent::SchedulingPolicy::Default, 0},
             std::vector<system_configuration::PortMapping>(
                 {system_configuration::PortMapping{"tick", "/tick"},
                  system_configuration::PortMapping{"tack", "/tack"}})}});
    EXPECT_THROW(configurator.configure(partialConfiguration), SystemConfigurationError);
    EXPECT_EQ(manager.getComponents().size(), 0);

    // test for correct inter-port typing in the complete configuration
    auto invalidTyping = system_configuration::ComponentSystem(
        {system_configuration::ComponentInstance{
             "producer",
             "esr/Producer",
             IComponent::SchedulingParameters{IComponent::SchedulingPolicy::Default, 0},
             std::vector<system_configuration::PortMapping>(
                 {system_configuration::PortMapping{"Trigger", "/trigger"},
                  system_configuration::PortMapping{"Counter", "/values"}})},
         system_configuration::ComponentInstance{
             "consumer",
             "esr/StringConsumer",
             IComponent::SchedulingParameters{IComponent::SchedulingPolicy::Default, 0},
             std::vector<system_configuration::PortMapping>(
                 {system_configuration::PortMapping{"Consumer", "/values"}})}});
    EXPECT_THROW(configurator.configure(partialConfiguration), SystemConfigurationError);
    EXPECT_EQ(manager.getComponents().size(), 0);

    auto configuration
        = system_configuration::ComponentSystem({system_configuration::ComponentInstance{
            "test",
            "esr/TestComponent",
            IComponent::SchedulingParameters{IComponent::SchedulingPolicy::Default, 0},
            std::vector<system_configuration::PortMapping>(
                {system_configuration::PortMapping{"tick", "/tick"},
                 system_configuration::PortMapping{"tack", "/tack"}})}});
    configurator.configure(configuration);

    EXPECT_EQ(manager.getComponents().size(), 1);
    EXPECT_EQ(manager.getComponents().at(0).name(), "test");

    manager.startup();

    // Invalid typing
    EXPECT_THROW(
        configurator.configure(
            system_configuration::ComponentSystem({system_configuration::ComponentInstance{
                "test",
                "esr/NoSuchComponent",
                IComponent::SchedulingParameters{IComponent::SchedulingPolicy::Default, 0},
                std::vector<system_configuration::PortMapping>(
                    {system_configuration::PortMapping{"tack", "/anotherTack"}})}})),
        SystemConfigurationError);
}

TEST_F(SystemConfigurationTest, InitializeFromJSON)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);
    mcf::ComponentSystemConfigurator configurator(manager, instantiator);

    instantiator.addComponentType(ComponentType::create<Producer>("esr/Producer"));
    instantiator.addComponentType(ComponentType::create<Consumer>("esr/Consumer"));
    instantiator.addComponentType(ComponentType::create<TestComponent>("esr/TestComponent"));

    std::string configuration
        = "{\"ComponentSystemConfiguration\": { \"Components\": {\"test\": {\"type\": "
          "\"esr/TestComponent\", "
          "\"portMapping\": { \"tick\": \"/tick\", \"tack\": \"/tack\" } } } } }";
    configurator.configureFromJSON(configuration);

    EXPECT_EQ(manager.getComponents().size(), 1);
    EXPECT_EQ(manager.getComponents().at(0).name(), "test");

    manager.startup();

    valueStore.setValue("/tick", TestValue(17));

    waitForValue(valueStore, "/tack");
    EXPECT_TRUE(valueStore.hasValue("/tack"));
    EXPECT_EQ(valueStore.getValue<TestValue>("/tack")->val, 17);

    configurator.configureFromJSON("{\"ComponentSystemConfiguration\": { \"Components\": {\"test\": {"
                                   "\"portMapping\": { \"tack\": \"/anotherTack\" } } } } }");

    valueStore.setValue("/tick", TestValue(42));
    waitForValue(valueStore, "/anotherTack");

    EXPECT_TRUE(valueStore.hasValue("/anotherTack"));
    EXPECT_EQ(valueStore.getValue<TestValue>("/anotherTack")->val, 42);
}

TEST_F(SystemConfigurationTest, Scheduling)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);
    mcf::ComponentSystemConfigurator configurator(manager, instantiator);

    instantiator.addComponentType(
        ComponentType::create<SchedulableComponent>("esr/SchedulableComponent"));

    std::string configuration
        = "{\"ComponentSystemConfiguration\": { \"Components\": {\"test\": {\"type\": "
          "\"esr/SchedulableComponent\", "
          "\"schedulingParameters\": { \"policy\": \"other\", \"priority\": 0 },"
          "\"portMapping\": { \"Counter\": \"/counter\", \"Trigger\": \"/tick\" } } } } }";
    configurator.configureFromJSON(configuration);

    manager.startup();

    valueStore.setValue("/tick", mcf::test::TestValue(17));

    waitForValue(valueStore, "/counter");
    EXPECT_TRUE(valueStore.hasValue("/counter"));
    EXPECT_EQ(valueStore.getValue<TestValue>("/counter")->val, 1);
}

TEST_F(SystemConfigurationTest, NullTopics)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);
    mcf::ComponentSystemConfigurator configurator(manager, instantiator);

    instantiator.addComponentType(ComponentType::create<Producer>("esr/Producer"));
    instantiator.addComponentType(ComponentType::create<Consumer>("esr/Consumer"));
    instantiator.addComponentType(ComponentType::create<TestComponent>("esr/TestComponent"));

    std::string configuration = "{\"ComponentSystemConfiguration\": { \"Components\": {\"test\": {\"type\": "
                                "\"esr/TestComponent\", "
                                "\"portMapping\": { \"tick\": null, \"tack\": \"/tack\" } } } } }";
    configurator.configureFromJSON(configuration);

    manager.startup();

    auto ports = manager.getComponents().at(0).ports();
    for (const auto& port : ports)
    {
        if (port.name() == "tick")
        {
            EXPECT_FALSE(port.isConnected());
            EXPECT_TRUE(port.topic().empty());
        }
        if (port.name() == "tack")
        {
            EXPECT_TRUE(port.isConnected());
            EXPECT_EQ(port.topic(), "/tack");
        }
    }

    valueStore.setValue("/tick", TestValue(17));

    waitForValue(valueStore, "/tack");
    EXPECT_FALSE(valueStore.hasValue("/tack"));
}

TEST_F(SystemConfigurationTest, DisabledPorts)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);
    mcf::ComponentSystemConfigurator configurator(manager, instantiator);

    instantiator.addComponentType(ComponentType::create<Producer>("esr/Producer"));
    instantiator.addComponentType(ComponentType::create<Consumer>("esr/Consumer"));
    instantiator.addComponentType(ComponentType::create<TestComponent>("esr/TestComponent"));

    std::string configuration = "{\"ComponentSystemConfiguration\": { \"Components\": {\"test\": {\"type\": "
                                "\"esr/TestComponent\", "
                                "\"portMapping\": { \"tick\": \"/tick\", \"tack\": {\"topic\": \"/tack\", \"connected\": false } } } } } }";
    configurator.configureFromJSON(configuration);

    manager.startup(false);

    auto component = manager.getComponents()[0];
    auto tickPort  = component.port("tick");
    auto tackPort  = component.port("tack");
    EXPECT_TRUE(tickPort.isConnected());
    EXPECT_EQ(tickPort.topic(), "/tick");

    EXPECT_FALSE(tackPort.isConnected());
    EXPECT_EQ(tackPort.topic(), "/tack");
}

} // namespace mcf