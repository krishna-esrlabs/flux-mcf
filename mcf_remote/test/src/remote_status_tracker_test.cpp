/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_remote/RemoteStatusTracker.h"
#include "mcf_remote/AbstractSender.h"

#include "gtest/gtest.h"

#include <thread>

namespace mcf {

namespace remote {

class RemoteStatusTrackerTest : public ::testing::Test {
public:
    class TestSender : public AbstractSender
    {
    public:
        std::string sendValue(const std::string& topic, ValuePtr value) override
        {
            return "";
        }
        void connect(){}
        void disconnect(){}
        void sendPing(const uint64_t freshnessValue) override
        {
            _freshnessValue = freshnessValue;
        }
        void sendPong(const uint64_t freshnessValue) override
        {
            _rst->pongReceived(freshnessValue);
        }
        void sendRequestAll() override
        {}
        std::string sendBlockedValueInjected(const std::string& topic) override
        {
            return "";
        }

        std::string sendBlockedValueRejected(const std::string& topic) override
        {
            return "";
        }

        void setRemoteStatusTracker(RemoteStatusTracker* rst)
        {
            _rst = rst;
        }
        bool connected() const override
        {
            return true;
        }
        void reply()
        {
            sendPong(_freshnessValue);
        }
        std::string connectionStr() const
        {
            return "";
        }
    private:
        RemoteStatusTracker* _rst;
        uint64_t _freshnessValue = 42ul;
    };
};

TEST_F(RemoteStatusTrackerTest, Simple)
{
    TestSender testSender;
    std::function<void(uint64_t)> pingSender =
        [&testSender](uint64_t fv) { testSender.sendPing(fv); };
    RemoteStatusTracker rst(
        pingSender,
        std::chrono::milliseconds(10),
        std::chrono::milliseconds(300),
        std::chrono::milliseconds(10));
    testSender.setRemoteStatusTracker(&rst);

    EXPECT_EQ(RemoteStatusTracker::STATE_UNSURE, rst.getState());

    rst.pongReceived(77ul);
    EXPECT_EQ(RemoteStatusTracker::STATE_UNSURE, rst.getState());

    rst.runCyclic();
    EXPECT_EQ(RemoteStatusTracker::STATE_UNSURE, rst.getState());

    testSender.reply();
    EXPECT_EQ(RemoteStatusTracker::STATE_UP, rst.getState());

    rst.runCyclic();
    EXPECT_EQ(RemoteStatusTracker::STATE_UP, rst.getState());

    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    rst.runCyclic();
    EXPECT_EQ(RemoteStatusTracker::STATE_DOWN, rst.getState());

    rst.messageReceivedInDown();
    EXPECT_EQ(RemoteStatusTracker::STATE_UNSURE, rst.getState());

    rst.runCyclic();
    EXPECT_EQ(RemoteStatusTracker::STATE_UNSURE, rst.getState());

    testSender.reply();
    EXPECT_EQ(RemoteStatusTracker::STATE_UP, rst.getState());

}

std::atomic<bool> run;
void runCyclic(RemoteStatusTracker& rst)
{
    while(run)
    {
        rst.runCyclic();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void waitForStateChange(
    const RemoteStatusTracker& rst,
    RemoteStatusTracker::RemoteState desiredState)
{
    for(int i = 0; i < 100; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if(rst.getState() == desiredState) break;
    }
}

TEST_F(RemoteStatusTrackerTest, MultiThreaded)
{
    TestSender testSender;
    std::function<void(uint64_t)> pingSender =
        [&testSender](uint64_t fv) { testSender.sendPing(fv); };
    RemoteStatusTracker rst(
        pingSender,
        std::chrono::milliseconds(100),
        std::chrono::milliseconds(1000),
        std::chrono::milliseconds(100));
    testSender.setRemoteStatusTracker(&rst);

    run = true;
    std::thread cyclicRunner(&runCyclic, std::ref(rst));

    EXPECT_EQ(RemoteStatusTracker::STATE_UNSURE, rst.getState());

    rst.pongReceived(77ul);

    EXPECT_EQ(RemoteStatusTracker::STATE_UNSURE, rst.getState());

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    EXPECT_EQ(RemoteStatusTracker::STATE_UNSURE, rst.getState());

    testSender.reply();

    waitForStateChange(rst, RemoteStatusTracker::STATE_UP);
    EXPECT_EQ(RemoteStatusTracker::STATE_UP, rst.getState());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(RemoteStatusTracker::STATE_UP, rst.getState());

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    EXPECT_EQ(RemoteStatusTracker::STATE_DOWN, rst.getState());

    // testSender.sendPing(961122ul);
    rst.messageReceivedInDown();
    waitForStateChange(rst, RemoteStatusTracker::STATE_UNSURE);
    EXPECT_EQ(RemoteStatusTracker::STATE_UNSURE, rst.getState());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(RemoteStatusTracker::STATE_UNSURE, rst.getState());

    testSender.reply();
    waitForStateChange(rst, RemoteStatusTracker::STATE_UP);
    EXPECT_EQ(RemoteStatusTracker::STATE_UP, rst.getState());

    run = false;
    cyclicRunner.join();
}

} // end namespace remote

} // end namespace mcf

