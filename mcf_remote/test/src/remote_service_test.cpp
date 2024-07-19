/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_remote/RemoteService.h"
#include "mcf_core/ExtMemValue.h"

#include "mcf_remote/RemoteService.h"
#include "mcf_remote/ZmqMsgPackSender.h"
#include "mcf_remote/ZmqMsgPackValueReceiver.h"
#include "mcf_remote/RemoteServiceUtils.h"

namespace mcf {

namespace remote {

class RemoteServiceTest : public ::testing::Test {
public:
    class TestValue : public mcf::Value {
    public:
        TestValue(int val=0) : val(val) {};
        int val;
        MSGPACK_DEFINE(val);
    };

    class ExtMemTestValue : public mcf::ExtMemValue<uint8_t> {
    public:
        MSGPACK_DEFINE();
    };

    class TestComponent : public Component {
    public:
        TestComponent(bool isSender) :
            Component(std::string("TestComponent") + (isSender ? "Sender" : "Receiver")),
            fSender(isSender),
            fSenderPort(*this, "Sender"),
            fReceiverPort(*this, "Receiver")
        {}

        void configure(IComponentConfig& config) {
            if (fSender) {
                config.registerPort(fSenderPort, "/test1");
            }
            else {
                config.registerPort(fReceiverPort, "/test1");
            }
        }

        bool fSender;
        SenderPort<TestValue> fSenderPort;
        ReceiverPort<TestValue> fReceiverPort;
    };

    template<typename T>
    inline void registerValueTypes(T& r) {
        r.template registerType<TestValue>("TestValue");
        r.template registerType<ExtMemTestValue>("ExtMemTestValue");
    }
};

TEST_F(RemoteServiceTest, LegacyInterface) {
    mcf::ValueStore vs1;
    mcf::ValueStore vs2;

    registerValueTypes(vs1);
    registerValueTypes(vs2);

    mcf::ComponentManager cm1(vs1);
    mcf::ComponentManager cm2(vs2);

    auto rs = std::make_shared<mcf::remote::RemoteSender>(vs1);
    auto rr = std::make_shared<mcf::remote::RemoteReceiver>(5555, vs2);
    auto testSender = std::make_shared<TestComponent>(true);
    auto testReceiver = std::make_shared<TestComponent>(false);

    rs->addSendRule<TestValue>("/test1", "tcp://localhost:5555", 1000);
    rr->addReceiveRule<TestValue>("/test1");

    cm1.registerComponent(rs);
    cm1.registerComponent(testSender);
    cm2.registerComponent(rr);
    cm2.registerComponent(testReceiver);

    cm1.configure();
    cm2.configure();

    cm1.startup();
    cm2.startup();

    // set a value 1000, these values shall be forwarded to vs2
    for (int i=0; i<1000; i++) {
        vs1.setValue("/test1", TestValue(i));
    }

    // if wait time is too short, receiver might not have enough time to receive
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // check if the last value is present on the port of vs2
    auto val = vs2.getValue<TestValue>("/test1")->val;
    ASSERT_EQ(999, val);

    cm1.shutdown();
    cm2.shutdown();
}

TEST_F(RemoteServiceTest, Simple) {
    mcf::ValueStore vs1;
    mcf::ValueStore vs2;

    registerValueTypes(vs1);
    registerValueTypes(vs2);

    mcf::ComponentManager cm1(vs1);
    mcf::ComponentManager cm2(vs2);

    // manually creating RemoteService
    std::unique_ptr<AbstractSender> zmqSender(
        new mcf::remote::ZmqMsgPackSender("tcp://localhost:5555", vs1));
    std::unique_ptr<AbstractReceiver<ValuePtr>> zmqReceiver(
        new mcf::remote::ZmqMsgPackValueReceiver("tcp://*:5556", vs1));
    auto sender = std::make_shared<mcf::remote::RemoteService>(
        vs1,
        RemotePair<ValuePtr>(std::move(zmqSender), std::move(zmqReceiver)));

    // creating RemoteService using helper function
    auto receiver = buildZmqRemoteService("tcp://localhost:5556", "tcp://*:5555", vs2);

    sender->addSendRule("/test1", "/test2", 1, true);
    receiver->addReceiveRule("/test3", "/test2");

    cm1.registerComponent(sender);
    cm2.registerComponent(receiver);

    cm1.configure();
    cm2.configure();

    cm1.startup();
    cm2.startup();

    // wait for remote services to connect
    for(int i = 0; i < 100; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(sender->connected() && receiver->connected()) break;
    }
    EXPECT_TRUE(sender->connected());
    EXPECT_TRUE(receiver->connected());

    // set a value 1000, these values shall be forwarded to vs2
    for (int i=0; i<1000; i++) {
        vs1.setValue("/test1", TestValue(i));
    }

    // if wait time is too short, receiver might not have enough time to receive
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // check if the last value is present on the port of vs2
    auto val = vs2.getValue<TestValue>("/test3")->val;
    ASSERT_EQ(999, val);

    cm1.shutdown();
    cm2.shutdown();
}

TEST_F(RemoteServiceTest, LegacyInterfaceExtMemValue) {
    mcf::ValueStore vs1;
    mcf::ValueStore vs2;

    registerValueTypes(vs1);
    registerValueTypes(vs2);

    mcf::ComponentManager cm1(vs1);
    mcf::ComponentManager cm2(vs2);

    auto rs = std::make_shared<mcf::remote::RemoteSender>(vs1);
    auto rr = std::make_shared<mcf::remote::RemoteReceiver>(5555, vs2);
    auto testSender = std::make_shared<TestComponent>(true);
    auto testReceiver = std::make_shared<TestComponent>(false);

    rs->addSendRule<TestValue>("/test1", "tcp://localhost:5555", 1000);
    rr->addReceiveRule<TestValue>("/test1");

    cm1.registerComponent(rs);
    cm1.registerComponent(testSender);
    cm2.registerComponent(rr);
    cm2.registerComponent(testReceiver);

    auto queue = std::make_shared<mcf::ValueQueue>();
    vs2.addReceiver("/test1", queue);

    cm1.configure();
    cm2.configure();

    cm1.startup();
    cm2.startup();

    // send 10 values with 5'000'000 bytes of extMem payload from one ValueStore to another
    const size_t testDataLength = 5'000'000;
    const size_t numTestItems = 10;

    for (size_t i=0; i<numTestItems; i++) {
        std::shared_ptr<ExtMemTestValue> extval(new ExtMemTestValue());
        extval->extMemInit(testDataLength);
        std::memset(extval->extMemPtr(), i, testDataLength);

        vs1.setValue("/test1", static_cast<ValuePtr>(extval));
    }

    // verify if the sent ExtMemValues have arrived
    for (size_t i=0; i<numTestItems; i++) {
        for (int j=0; j<1000 && queue->empty(); j++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        EXPECT_FALSE(queue->empty());
        auto value = queue->pop<ExtMemTestValue>();
        const uint8_t* data = value->extMemPtr();
        for (size_t j=0; j<testDataLength; j++) {
            ASSERT_EQ(i, data[j]);
        }
    }

    cm1.shutdown();
    cm2.shutdown();
}

TEST_F(RemoteServiceTest, ExtMemValue) {
    mcf::ValueStore vs1;
    mcf::ValueStore vs2;

    registerValueTypes(vs1);
    registerValueTypes(vs2);

    mcf::ComponentManager cm1(vs1);
    mcf::ComponentManager cm2(vs2);

    auto sender = buildZmqRemoteService("ipc:///tmp/0", "ipc:///tmp/1", vs1);
    auto receiver = buildZmqRemoteService("ipc:///tmp/1", "ipc:///tmp/0", vs2);

    sender->addSendRule("/test1", "/test2", 1, true);
    receiver->addReceiveRule("/test3", "/test2");

    cm1.registerComponent(sender);
    cm2.registerComponent(receiver);

    auto queue = std::make_shared<mcf::ValueQueue>();
    vs2.addReceiver("/test3", queue);

    cm1.configure();
    cm2.configure();

    cm1.startup();
    cm2.startup();

    // wait for remote services to connect
    for(int i = 0; i < 100; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(sender->connected() && receiver->connected()) break;
    }
    EXPECT_TRUE(sender->connected());
    EXPECT_TRUE(receiver->connected());

    // send 10 values with 5'000'000 bytes of extMem payload from one ValueStore to another
    const size_t testDataLength = 5'000'000;
    const size_t numTestItems = 10;

    for (size_t i=0; i<numTestItems; i++) {
        std::shared_ptr<ExtMemTestValue> extval(new ExtMemTestValue());
        extval->extMemInit(testDataLength);
        std::memset(extval->extMemPtr(), i, testDataLength);

        vs1.setValue("/test1", static_cast<ValuePtr>(extval));
    }

    // verify if the sent ExtMemValues have arrived
    for (size_t i=0; i<numTestItems; i++) {
        for (int j=0; j<1000 && queue->empty(); j++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        EXPECT_FALSE(queue->empty());
        auto value = queue->pop<ExtMemTestValue>();
        const uint8_t* data = value->extMemPtr();
        for (size_t j=0; j<testDataLength; j++) {
            ASSERT_EQ(i, data[j]);
        }
    }

    cm1.shutdown();
    cm2.shutdown();
}

} // end namespace remote

} // end namespace mcf

