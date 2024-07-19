/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/LoggingMacros.h"

#include "mcf_remote/ZmqMsgPackSender.h"
#include "mcf_remote/ZmqMsgPackValueReceiver.h"
#include "mcf_remote/IComEventListener.h"
#include "mcf_remote/ShmemKeeper.h"
#include "mcf_remote/ShmemClient.h"

#include "mcf_core/ExtMemValue.h"

#include "gtest/gtest.h"

#include <thread>

namespace mcf {

namespace remote {

class ZmqMsgPackTest : public ::testing::Test {
public:
    class TestValue : public mcf::Value {
    public:
        TestValue(int val=0) : val(val) {};
        int val;
        MSGPACK_DEFINE(val);
    };

    class ExtMemTestValue : public mcf::ExtMemValue<int32_t> {
    public:
        MSGPACK_DEFINE();
    };

    void initExtMem(ExtMemTestValue& emtv, const uint64_t len)
    {
        emtv.extMemInit(len * sizeof(int32_t));
        int32_t* extMemPtr = reinterpret_cast<int32_t*>(emtv.extMemPtr());
        for(int i = 0; i < len; ++i) {
            extMemPtr[i] = i;
        }
    }

    void checkExtMem(std::shared_ptr<const ExtMemTestValue> emtv, const uint64_t len)
    {
        ASSERT_NE(nullptr, emtv.get());
        EXPECT_EQ(len * sizeof(int32_t), emtv->extMemSize());
        const int32_t* extMemPtr1 = reinterpret_cast<const int32_t*>(emtv->extMemPtr());
        for(int i = 0; i < len/sizeof(int32_t); ++i) {
            EXPECT_EQ(extMemPtr1[i], i);
        }
    }

    class ComEventListener : public IComEventListener<ValuePtr>
    {
    public:
        ComEventListener() : _pingValue(0)
        {}

        std::string valueReceived(const std::string& topic, ValuePtr value) override
        {
            EXPECT_TRUE("TestValue" == topic || "ExtMemTestValue" == topic);
            if(topic == "TestValue") testValue = value;
            if(topic == "ExtMemTestValue") extMemTestValue = value;

            return "INJECTED";
        }

        void pingReceived(uint64_t freshnessValue) override
        {
            _pingValue = freshnessValue;
        }

        void pongReceived(uint64_t freshnessValue) override
        {
            pongFreshnessValue = freshnessValue;
        }

        void requestAllReceived() override
        {
            requestedAll = true;
        }

        void blockedValueInjectedReceived(const std::string& topic) override
        {
            injected = topic;
        }

        void blockedValueRejectedReceived(const std::string& topic) override
        {
            rejected = topic;
        }

        uint64_t freshnessValue()
        {
            return _pingValue.load();
        }

        uint64_t pongFreshnessValue = 0ul;
        ValuePtr testValue = nullptr;
        ValuePtr extMemTestValue = nullptr;
        bool requestedAll = false;
        std::string injected;
        std::string rejected;

    private:
        std::atomic<uint64_t> _pingValue;
    };

    template<typename T>
    inline void registerValueTypes(T& r)
    {
        r.template registerType<TestValue>("TestValue");
        r.template registerType<ExtMemTestValue>("ExtMemTestValue");
    }
};

TEST_F(ZmqMsgPackTest, Simple)
{
    ValueStore vs;

    ZmqMsgPackSender sender("ipc:///tmp/0", vs, std::chrono::milliseconds(1000));

    sender.connect();

    std::shared_ptr<const TestValue> value = std::make_shared<TestValue>(940824);
    std::string response = sender.sendValue("TestValue", value);

    // REJECTED because the type to be sent is not registered (yet)
    EXPECT_EQ("REJECTED", response);

    registerValueTypes(vs);
    response = sender.sendValue("TestValue", value);

    // TIMEOUT because there is no receiver (yet)
    EXPECT_EQ("TIMEOUT", response);

    // Re-connect to clear messages lingering in zmq buffers
    sender.disconnect();
    sender.connect();

    ZmqMsgPackValueReceiver receiver("ipc:///tmp/0", vs);
    ComEventListener cel;
    receiver.setEventListener(&cel);

    std::atomic<bool> success(false);

    // Start receiver
    std::thread rt([&receiver, &success](){
        receiver.connect();
        // Try to receive a message for 1 second
        success = receiver.receive(std::chrono::milliseconds(1000));
        // after the event listener has been removed, all messages will be rejected
        receiver.removeEventListener();
        // Try to receive a message for 1 second
        success = receiver.receive(std::chrono::milliseconds(1000));
        receiver.disconnect();
    });

    response = sender.sendValue("TestValue", value);

    // Transmission should have been successful now
    EXPECT_EQ("INJECTED", response);
    EXPECT_TRUE(success);

    response = sender.sendValue("TestValue", value);

    // Transmission should still succeed but the value be rejected after listener removal
    EXPECT_EQ("REJECTED", response);
    EXPECT_TRUE(success);
    rt.join();

    sender.disconnect();
}

void receive(ZmqMsgPackValueReceiver& receiver, uint32_t num, std::condition_variable& cv)
{
    receiver.connect();
    uint32_t received = 0;

    for(int i = 0; i < 100; ++i)
    {
        cv.notify_all();
        bool success = receiver.receive(std::chrono::milliseconds(50));
        if(success)
        {
            ++received;
            if(received >= num)
            {
                break;
            }
        }
    }

    receiver.disconnect();
}

TEST_F(ZmqMsgPackTest, PingPong)
{
    ValueStore vs;

    ZmqMsgPackSender sender("ipc:///tmp/0", vs);
    ZmqMsgPackValueReceiver receiver("ipc:///tmp/0", vs);

    ComEventListener cel;
    receiver.setEventListener(&cel);

    std::mutex mtx;
    std::condition_variable cv;

    sender.connect();

    std::thread receiveMsgs(&receive, std::ref(receiver), 2, std::ref(cv));

    // wait for receiver to be set up;
    {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk);
    }
    uint64_t freshnessValue = 77ul;

    // send a ping which shall be automatically replied with a pong
    sender.sendPing(freshnessValue);

    // wait for the ping to reach the event listener
    for(int i = 0; i < 10; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(cel.freshnessValue() != 0)
        {
            break;
        }
    }

    ASSERT_EQ(freshnessValue, cel.freshnessValue());
    sender.sendPong(cel.freshnessValue());

    receiveMsgs.join();

    sender.disconnect();

    // check if ping/pong was successful
    ASSERT_EQ(freshnessValue, cel.pongFreshnessValue);
}

TEST_F(ZmqMsgPackTest, Value)
{
    ValueStore vs;
    registerValueTypes(vs);

    ZmqMsgPackSender sender("ipc:///tmp/0", vs);
    ZmqMsgPackValueReceiver receiver("ipc:///tmp/0", vs);

    ComEventListener cel;
    receiver.setEventListener(&cel);

    std::mutex cv_m;
    std::condition_variable cv;

    std::thread receiveValues(&receive, std::ref(receiver), 2, std::ref(cv));

    // wait for receiver to be set up;
    {
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk);
    }

    // connect the sender
    EXPECT_FALSE(sender.connected());
    sender.connect();
    EXPECT_TRUE(sender.connected());

    // create and send a test value
    std::shared_ptr<const TestValue> value = std::make_shared<const TestValue>(940824);
    std::string response = sender.sendValue("TestValue", value);
    EXPECT_EQ("INJECTED", response);

    // create and send an extMem test value
    const uint64_t len = 768;
    ExtMemTestValue extMemValue;
    initExtMem(extMemValue, len);
    response = sender.sendValue(
        "ExtMemTestValue",
        std::make_shared<const ExtMemTestValue>(std::move(extMemValue)));
    EXPECT_EQ("INJECTED", response);


    // check if values have been received
    receiveValues.join();
    sender.disconnect();
    EXPECT_FALSE(sender.connected());

    EXPECT_NE(nullptr, cel.testValue.get());
    EXPECT_NE(nullptr, cel.extMemTestValue.get());


    std::shared_ptr<const TestValue> celTestValue =
        std::dynamic_pointer_cast<const TestValue>(cel.testValue);
    ASSERT_NE(nullptr, celTestValue.get());
    EXPECT_EQ(value->val, celTestValue->val);

    std::shared_ptr<const ExtMemTestValue> celExtMemTestValue =
        std::dynamic_pointer_cast<const ExtMemTestValue>(cel.extMemTestValue);
    checkExtMem(celExtMemTestValue, len);
}

TEST_F(ZmqMsgPackTest, Commands) {
    ValueStore vs;

    ZmqMsgPackSender sender("tcp://localhost:5555", vs);
    ZmqMsgPackValueReceiver receiver("tcp://*:5555", vs);

    ComEventListener cel;
    receiver.setEventListener(&cel);

    std::mutex cv_m;
    std::condition_variable cv;

    EXPECT_FALSE(sender.connected());
    sender.connect();
    EXPECT_TRUE(sender.connected());
    sender.connect();
    EXPECT_TRUE(sender.connected());

    std::thread receiveMsgs(&receive, std::ref(receiver), 3, std::ref(cv));

    // wait for receiver to be set up;
    {
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk);
    }

    // send command "request all
    sender.sendRequestAll();

    // send command "rejected"
    std::string topicToReject("rejectedTopic");
    sender.sendBlockedValueRejected(topicToReject);

    // send command "injected"
    std::string topicToInject("injectedTopic");
    sender.sendBlockedValueInjected(topicToInject);

    receiveMsgs.join();
    sender.disconnect();
    EXPECT_FALSE(sender.connected());

    // check if "request all" command has been received
    ASSERT_TRUE(cel.requestedAll);

    // check if "injected" command has been received
    ASSERT_EQ(topicToInject, cel.injected);

    // check if "rejected" command has been received
    ASSERT_EQ(topicToReject, cel.rejected);
}

TEST_F(ZmqMsgPackTest, ValueShm)
{
    ValueStore vs;
    registerValueTypes(vs);

    // Create a sender using the shared memory protocol
    std::shared_ptr<mcf::remote::ShmemKeeper> shmemKeeper(new mcf::remote::SingleFileShmem());
    ZmqMsgPackSender sender("shm://0", vs, std::chrono::milliseconds(100), shmemKeeper);

    // Create a receiver using the shared memory protocol
    std::shared_ptr<mcf::remote::ShmemClient> shmemClient(new mcf::remote::ShmemClient());
    ZmqMsgPackValueReceiver receiver("shm://0", vs, shmemClient);

    ComEventListener cel;
    receiver.setEventListener(&cel);

    std::mutex cv_m;
    std::condition_variable cv;

    std::thread receiveValues(&receive, std::ref(receiver), 2, std::ref(cv));

    // wait for receiver to be set up;
    {
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk);
    }

    EXPECT_FALSE(sender.connected());
    sender.connect();
    EXPECT_TRUE(sender.connected());

    // verify the shared memory client is still valid
    EXPECT_TRUE(shmemClient.get());

    // sending a simple value over shm
    // using ipc, since there is no extMem part
    std::shared_ptr<const TestValue> value = std::make_shared<const TestValue>(940824);
    std::string response = sender.sendValue("TestValue", value);
    EXPECT_EQ("INJECTED", response);

    // sending an extMem value over shm
    // using ipc for the value and shared memory for the extMem part
    const uint64_t len = 768;
    ExtMemTestValue extMemValue;
    initExtMem(extMemValue, len);
    response = sender.sendValue(
        "ExtMemTestValue",
        std::make_shared<const ExtMemTestValue>(std::move(extMemValue)));
    EXPECT_EQ("INJECTED", response);


    // check if the values have been received
    receiveValues.join();
    sender.disconnect();
    EXPECT_FALSE(sender.connected());

    EXPECT_NE(nullptr, cel.testValue.get());
    EXPECT_NE(nullptr, cel.extMemTestValue.get());


    std::shared_ptr<const TestValue> celTestValue =
        std::dynamic_pointer_cast<const TestValue>(cel.testValue);
    ASSERT_NE(nullptr, celTestValue.get());
    EXPECT_EQ(value->val, celTestValue->val);

    std::shared_ptr<const ExtMemTestValue> celExtMemTestValue =
        std::dynamic_pointer_cast<const ExtMemTestValue>(cel.extMemTestValue);
    checkExtMem(celExtMemTestValue, len);
}

} // end namespace remote

} // end namespace mcf

