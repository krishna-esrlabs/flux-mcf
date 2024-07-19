/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_core/Mcf.h"
#include "mcf_core/ExtMemValue.h"

#include <unistd.h>

namespace mcf {

class ComponentTest : public ::testing::Test {
public:

    class TestValue;
    class TestValueExtMem;

    class TestComponent : public Component {

    public:
        TestComponent() :
            Component("TestComponent"),
            fTickPort(*this, "Tick"),
            fTackPort(*this, "Tack")
        {
            fTickPort.registerHandler(std::bind(&TestComponent::tick, this));
            fTackPort.registerHandler(std::bind(&TestComponent::tack, this));
        }

        void configure(IComponentConfig& config) {
            config.registerPort(fTickPort, "/tick");
            config.registerPort(fTackPort, "/tack");
        }

        void tick() {
            auto value = fTickPort.getValue();
            events.push_back(std::string("/tick:") + std::to_string(value->val));
            ids.push_back(value->id());
        }

        void tack() {
            auto value = fTackPort.getValue();
            events.push_back(std::string("/tack:") + std::to_string(value->val));
            ids.push_back(value->id());
        }

        std::vector<std::string> events;
        std::vector<uint64_t> ids;
        ReceiverPort<TestValue> fTickPort;
        ReceiverPort<TestValue> fTackPort;
    };

    class QueuedTestComponent : public Component {

    public:
        QueuedTestComponent() :
            Component("QueuedTestComponent"),
            fTickPort(*this, "Tick", 0, true),
            fTackPort(*this, "Tack", 4, true)
        {
            fTickPort.registerHandler(std::bind(&QueuedTestComponent::tick, this));
            fTackPort.registerHandler(std::bind(&QueuedTestComponent::tack, this));
        }

        void configure(IComponentConfig& config) {
            config.registerPort(fTickPort, "/tick");
            config.registerPort(fTackPort, "/tack");
        }

        void tick() {
            while(fTickPort.hasValue())
            {
                auto value = fTickPort.getValue();
                tickEvents.push_back(std::string("/tick:") + std::to_string(value->val));
            }
        }

        void tack() {
            while(fTackPort.hasValue())
            {
                auto value = fTackPort.getValue();
                tackEvents.push_back(std::string("/tack:") + std::to_string(value->val));
            }
        }

        bool queuesEmpty() {
            return !fTickPort.hasValue() && !fTackPort.hasValue();
        }

        std::vector<std::string> tickEvents, tackEvents;
        QueuedReceiverPort<TestValue> fTickPort;
        QueuedReceiverPort<TestValue> fTackPort;
    };

    class TestComponent2 : public Component {
    public:
        TestComponent2() :
            Component("TestComponent2"),
            fTickPort(*this, "Tick"),
            fTackPort(*this, "Tack")
        {}

        void configure(IComponentConfig& config) {
            config.registerPort(fTickPort, "/tick");
            config.registerPort(fTackPort, "/tack");
        }

        void tick(TestValue& tv) {
            fTickPort.setValue(tv);
        }

        void tick(std::shared_ptr<const TestValue> tv) {
            fTickPort.setValue(tv);
        }

        void tack(TestValue& tv) {
            fTackPort.setValue(std::move(tv));
        }

        void tack(std::shared_ptr<const TestValue> tv) {
            fTackPort.setValue(std::move(tv));
        }

        SenderPort<TestValue> fTickPort;
        SenderPort<TestValue> fTackPort;
    };

    class TestComponent3 : public Component {
    public:
        TestComponent3() :
            Component("TestComponent3"),
            fExtMemPort(*this, "ExtMem")
        {}

        void configure(IComponentConfig& config) {
            config.registerPort(fExtMemPort, "/extmem");
        }

        void startup() {
            fExtMemPort.setValue(TestValueExtMem(5));
            TestValueExtMem tv2(7);
            fExtMemPort.setValue(std::move(tv2));
        }

        SenderPort<TestValueExtMem> fExtMemPort;
    };

    class TestComponent4 : public Component {
    public:
        TestComponent4() : Component("TestComponent4"), fThreadHandle(0){}

        void configure(IComponentConfig& config) {
        }

        void startup() { fThreadHandle = pthread_self(); }

        std::atomic<pthread_t> fThreadHandle;
    };

    class TestValue : public mcf::Value {
    public:
        TestValue(int val=0) : val(val) {};
        int val;
        MSGPACK_DEFINE(val);
    };

    class TestValueExtMem : public mcf::ExtMemValue<uint8_t> {
    public:
        TestValueExtMem(int val=0) : val(val) {};
        int val;
        MSGPACK_DEFINE(val);
    };
};

TEST_F(ComponentTest, ExtMemValue) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto testComponent3 = std::make_shared<ComponentTest::TestComponent3>();

    manager.registerComponent(testComponent3);

    manager.configure();
    manager.startup();

    while (!valueStore.hasValue("/extmem")) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    auto ptr = valueStore.getValue<TestValueExtMem>("/extmem");
    EXPECT_EQ(7, ptr->val);

    manager.shutdown();
}

TEST_F(ComponentTest, Simple) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto testComponent = std::make_shared<ComponentTest::TestComponent>();
    auto testComponent2 = std::make_shared<ComponentTest::TestComponent2>();

    manager.registerComponent(testComponent);
    manager.registerComponent(testComponent2);

    manager.configure();
    manager.startup();

    testComponent->events.clear();

    valueStore.setValue("/tick", TestValue(1));
    valueStore.setValue("/tick", TestValue(2));
    valueStore.setValue("/tack", TestValue(2));

    // Wait up to 10 seconds to get results.
    // We expect to reive at least one value per connected port in this time
    for(int i = 0; i < 100; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(testComponent->events.size() >= 2) break;
    }

    ASSERT_LE(2, testComponent->events.size());
    ASSERT_GE(3, testComponent->events.size());

    const std::set<std::string> validEvents = {"/tick:1", "/tick:2", "/tack:2"};
    auto eventChecker = [&validEvents, &manager](const std::string& event)
    {
        if(validEvents.find(event) == validEvents.end())
        {
            MCF_ERROR("Received unexpected event: {}", event);

            manager.shutdown(); // shut down manager to avoid test to get stuck
            return false;
        }
        return true;
    };
    for(const std::string& event : testComponent->events)
    {
        ASSERT_TRUE(eventChecker(event));
    }

    manager.shutdown();
}

TEST_F(ComponentTest, QueuedPorts) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto testComponent = std::make_shared<ComponentTest::QueuedTestComponent>();
    auto testComponent2 = std::make_shared<ComponentTest::TestComponent2>();

    manager.registerComponent(testComponent);
    manager.registerComponent(testComponent2);

    manager.configure();
    manager.startup();

    testComponent->tickEvents.clear();
    testComponent->tackEvents.clear();
    ASSERT_TRUE(testComponent->queuesEmpty());

    std::string tick = "/tick";
    std::string tack = "/tack";

    valueStore.setValue(tick, TestValue(1));
    valueStore.setValue(tack, TestValue(1));
    valueStore.setValue(tick, TestValue(2));
    valueStore.setValue(tack, TestValue(2));
    valueStore.setValue(tick, TestValue(3));
    valueStore.setValue(tack, TestValue(3));
    valueStore.setValue(tick, TestValue(4));
    valueStore.setValue(tack, TestValue(4));

    // Wait up to 10 seconds to get results.
    for(int i = 0; i < 100; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(testComponent->tickEvents.size() == 4 && testComponent->tackEvents.size() == 4) break;
    }

    ASSERT_TRUE(testComponent->queuesEmpty());
    ASSERT_EQ(4, testComponent->tickEvents.size());
    ASSERT_EQ(4, testComponent->tackEvents.size());

    tick += std::string(":");
    int idx = 0;
    for(const std::string& event : testComponent->tickEvents)
    {
        ASSERT_EQ(tick + std::to_string(++idx), event);
    }

    tack += std::string(":");
    idx = 0;
    for(const std::string& event : testComponent->tackEvents)
    {
        ASSERT_EQ(tack + std::to_string(++idx), event);
    }

    manager.shutdown();
}

class ConstantIdGenerator : public IidGenerator
{
public:
    virtual void injectId(Value& value) const override
    {
        setId((value), _id);
    }
private:
    uint64_t _id = 42ul;
};

TEST_F(ComponentTest, CustomIdGenerator) {
    mcf::ValueStore valueStore;
    std::shared_ptr<IidGenerator> idGenerator = std::make_shared<ConstantIdGenerator>();

    mcf::ComponentManager manager(valueStore, ComponentManager::DEFAULT_CONFIG_DIR, idGenerator);

    auto testComponent = std::make_shared<ComponentTest::TestComponent>();
    auto testComponent2 = std::make_shared<ComponentTest::TestComponent2>();

    manager.registerComponent(testComponent);
    manager.registerComponent(testComponent2);

    manager.configure();
    manager.startup();

    testComponent->setIdGenerator(std::make_shared<DefaultIdGenerator>());
    testComponent->events.clear();

    TestValue tv1(1);
    std::shared_ptr<const TestValue> sptv1(testComponent->createValue(tv1));
    testComponent2->tick(sptv1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TestValue tv2(2);
    testComponent2->tack(tv2);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TestValue tv3(3);
    testComponent2->tick(tv3);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::unique_ptr<TestValue> tv4(std::make_unique<TestValue>(TestValue(4)));
    std::shared_ptr<const TestValue> sptv4(testComponent2->createValue(tv4));
    ASSERT_EQ(nullptr, tv4.get());
    testComponent2->tack(sptv4);


    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_EQ(tv1.id(), sptv1->id());
    ASSERT_EQ(42, tv2.id());
    ASSERT_EQ(42, tv3.id());
    ASSERT_EQ(42, sptv4->id());

    for(uint64_t id : testComponent->ids)
    {
        ASSERT_TRUE(42 == id || tv1.id() == id);
    }

    manager.shutdown();
}

//// Lifecycle tests
TEST_F(ComponentTest, SimpleWithRestarts)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    
    auto testComponent = std::make_shared<ComponentTest::TestComponent>();
    auto testComponent2 = std::make_shared<ComponentTest::TestComponent2>();

    auto tc1Desc = manager.registerComponent(testComponent);
    auto tc2Desc = manager.registerComponent(testComponent2);

    EXPECT_EQ(tc1Desc.name().substr(0, 13), "TestComponent");
    EXPECT_EQ(tc2Desc.name().substr(0, 14), "TestComponent2");

    manager.configure();
    manager.startup();

    EXPECT_EQ(tc1Desc.ports().size(), 2 + 4); // two custom, log, log control, config in, config out

    for (const auto& p: tc1Desc.ports())
    {
        EXPECT_TRUE(p.isConnected());
    }

    // shutdown and startup first component
    manager.shutdown(tc1Desc);
    manager.startup(tc1Desc);

    testComponent->events.clear();

    valueStore.setValue("/tick", TestValue(1));
    valueStore.setValue("/tick", TestValue(2));
    valueStore.setValue("/tack", TestValue(2));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Wait up to 10 seconds to get results.
    // We expect to receive at least one value per connected port in this time
    for(int i = 0; i < 100; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(testComponent->events.size() >= 2) break;
    }

    ASSERT_LE(2, testComponent->events.size());
    ASSERT_GE(3, testComponent->events.size());

    const std::set<std::string> validEvents = {"/tick:1", "/tick:2", "/tack:2"};
    auto eventChecker                       = [&validEvents](const std::string& event) {
        if (validEvents.find(event) == validEvents.end())
        {
            MCF_ERROR("Received unexpected event: {}", event);
            return false;
        }
        return true;
    };
    for (const std::string& event : testComponent->events)
    {
        ASSERT_TRUE(eventChecker(event));
    }

    // shutdown first component
    manager.shutdown(tc1Desc);

    auto events = testComponent->events.size();
    valueStore.setValue("/tick", TestValue(3));
    for (int i = 0; i < 10; ++i)
    {
        if(testComponent->events.size() != events)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // expect no events coming through
    EXPECT_EQ(testComponent->events.size(), events);

    // start first component again
    manager.startup(tc1Desc);

    valueStore.setValue("/tick", TestValue(3));
    for (int i = 0; i < 100; ++i)
    {
        if(testComponent->events.size() > events)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // expect next event
    EXPECT_EQ(testComponent->events.size(), events + 1);

    manager.shutdown();
}

TEST_F(ComponentTest, SimpleWithReInstatination)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    
    auto testComponent = std::make_shared<ComponentTest::TestComponent>();
    auto testComponent2 = std::make_shared<ComponentTest::TestComponent2>();

    auto tc1Desc = manager.registerComponent(testComponent);
    auto tc2Desc = manager.registerComponent(testComponent2);

    EXPECT_EQ(tc1Desc.name().substr(0, 13), "TestComponent");
    EXPECT_EQ(tc2Desc.name().substr(0, 14), "TestComponent2");

    manager.configure();
    manager.startup();

    testComponent->events.clear();

    valueStore.setValue("/tick", TestValue(1));
    valueStore.setValue("/tick", TestValue(2));
    valueStore.setValue("/tack", TestValue(2));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Wait up to 10 seconds to get results.
    // We expect to receive at least one value per connected port in this time
    for(int i = 0; i < 100; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(testComponent->events.size() >= 2) break;
    }

    ASSERT_LE(2, testComponent->events.size());
    ASSERT_GE(3, testComponent->events.size());

    const std::set<std::string> validEvents = {"/tick:1", "/tick:2", "/tack:2"};
    auto eventChecker                       = [&validEvents](const std::string& event) {
        if (validEvents.find(event) == validEvents.end())
        {
            MCF_ERROR("Received unexpected event: {}", event);
            return false;
        }
        return true;
    };
    for (const std::string& event : testComponent->events)
    {
        ASSERT_TRUE(eventChecker(event));
    }

    // erase first component
    manager.eraseComponent(tc1Desc);

    auto events = testComponent->events.size();
    valueStore.setValue("/tick", TestValue(3));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // expect no events coming through
    EXPECT_EQ(testComponent->events.size(), events);

    // start first component again
    auto tc1Desc2 = manager.registerComponent(testComponent);
    EXPECT_THROW(manager.startup(tc1Desc), std::out_of_range);
    manager.configure(tc1Desc2);
    manager.startup(tc1Desc2);

    valueStore.setValue("/tick", TestValue(4));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // expect next event
    EXPECT_EQ(testComponent->events.size(), events + 1);
    EXPECT_EQ(testComponent->events[events], "/tick:4");

    manager.shutdown();
}

TEST_F(ComponentTest, SimpleWithExceptions) {
    try {
        mcf::ValueStore valueStore;
        mcf::ComponentManager manager(valueStore);

        auto testComponent = std::make_shared<ComponentTest::TestComponent>();
        auto testComponent2 = std::make_shared<ComponentTest::TestComponent2>();

        manager.registerComponent(testComponent);
        manager.registerComponent(testComponent2);

        manager.configure();
        manager.startup();

        testComponent->events.clear();

        // Components should run in their respective threads
        auto invalid = ComponentProxy("invalid", 1337, manager);
        manager.startup(invalid);

        manager.shutdown();
        FAIL() << "We should not get here";
    } catch (std::exception e) {
        // okay, this should be kinda successful
    }
}

TEST_F(ComponentTest, ThreadPriority) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    
    auto testComponent = std::make_shared<ComponentTest::TestComponent4>();

    // test validation
    int minPriority = sched_get_priority_min(SCHED_FIFO);
    int maxPriority = sched_get_priority_max(SCHED_FIFO);
    EXPECT_THROW(
        testComponent->ctrlSetSchedulingParameters(
            mcf::IComponent::SchedulingParameters{mcf::IComponent::Other, -95}),
        std::runtime_error);
    EXPECT_THROW(
        testComponent->ctrlSetSchedulingParameters(
            mcf::IComponent::SchedulingParameters{mcf::IComponent::Fifo, maxPriority + 1}),
        std::runtime_error);
    EXPECT_THROW(
        testComponent->ctrlSetSchedulingParameters(
            mcf::IComponent::SchedulingParameters{mcf::IComponent::RoundRobin, minPriority - 1}),
        std::runtime_error);

    // set priority to some value
    testComponent->ctrlSetSchedulingParameters(mcf::IComponent::SchedulingParameters{mcf::IComponent::Other, 0});
    // start the component

    manager.registerComponent(testComponent);

    manager.configure();
    manager.startup();

    while (testComponent->fThreadHandle.load() == 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // retrieve the thread priority
    int schedulingPolicy = 0;
    sched_param schedulingParameters = {0};

    pthread_getschedparam(testComponent->fThreadHandle.load(), &schedulingPolicy, &schedulingParameters);
    EXPECT_EQ(schedulingPolicy, SCHED_OTHER);
    EXPECT_EQ(schedulingParameters.sched_priority, 0);

    // check if we actually can set the thread priority to real-time
    schedulingParameters.sched_priority = 1;
    int ret = pthread_setschedparam(pthread_self(), SCHED_RR, &schedulingParameters);
    if (ret == EPERM)
    {
        MCF_ERROR_NOFILELINE("Cannot test component thread priority API, no permissions");
    }
    else
    {
        // lower own priority
        schedulingParameters.sched_priority = 0;
        pthread_setschedparam(pthread_self(), SCHED_OTHER, &schedulingParameters);
        // change the thread priority via the proxy interface
        auto proxy = manager.getComponents()[0];

        proxy.setSchedulingParameters(
            mcf::IComponent::SchedulingParameters{mcf::IComponent::SchedulingPolicy::RoundRobin, 17});
        // component is already started, so expect the scheduling priority to be set
        pthread_getschedparam(testComponent->fThreadHandle.load(), &schedulingPolicy, &schedulingParameters);
        EXPECT_EQ(schedulingPolicy, SCHED_RR);
        EXPECT_EQ(schedulingParameters.sched_priority, 17);
    }
    manager.shutdown();
}

}
