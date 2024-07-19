/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_core/TimeService.h"
#include <mutex>

namespace mcf {

class TimeServiceTest : public ::testing::Test
{

public:
    class TestComponent : public Component
    {

    public:
        TestComponent() :
            Component("TestComponent"),
            fTickPort(*this, "Tick")
        {}

        void startup()
        {
            fTickPort.registerHandler(std::bind(&TestComponent::tick, this));
        }

        void configure(IComponentConfig& config)
        {
            config.registerPort(fTickPort, "/time/10ms");
        }

        void tick()
        {
            std::lock_guard<std::mutex> lk(fMutex);
            events.push_back(std::string("/time/10ms"));
        }

        void clearTicks()
        {
            std::lock_guard<std::mutex> lk(fMutex);
            events.clear();
        }

        size_t numTicks()
        {
            std::lock_guard<std::mutex> lk(fMutex);
            return events.size();
        }

    private:

        std::vector<std::string> events;
        ReceiverPort<msg::Timestamp> fTickPort;
        std::mutex fMutex;
  };

};

TEST_F(TimeServiceTest, Simple)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto ts = std::make_shared<mcf::TimeService>();

    auto testComponent = std::make_shared<TimeServiceTest::TestComponent>();

    manager.registerComponent(ts);
    manager.registerComponent(testComponent);

    testComponent->clearTicks();

    manager.configure();
    manager.startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(110));

    manager.shutdown();

    // wait max 3 sec to see 10 ticks
    for (int i=0; testComponent->numTicks() < 10 && i<30; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ASSERT_TRUE(testComponent->numTicks() >= 9);
    ASSERT_TRUE(testComponent->numTicks() <= 13);
}

}



