/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_core/ValueStore.h"
#include "mcf_core/ComponentManager.h"
#include "mcf_core/Component.h"
#include "mcf_remote/RemoteService.h"
#include "mcf_remote/RemoteServiceConfigurator.h"

#include "json/json.h"

namespace mcf
{

namespace remote
{

class RemoteServiceConfiguratorTest : public ::testing::Test {
public:
    class TestValue : public mcf::Value {
    public:
        TestValue(int val=0) : val(val) {};
        int val;
        MSGPACK_DEFINE(val);
    };

    class TestSender : public Component {
    public:
        TestSender() :
            Component("TestSender")
        {}

        void configure(IComponentConfig& config) {
            _senderPorts.emplace_back(*this, "local");
            _senderPorts.emplace_back(*this, "remote");
            _senderPorts.emplace_back(*this, "same");
            _senderPorts.emplace_back(*this, "different");

            config.registerPort(_senderPorts[0], "/test/local");
            config.registerPort(_senderPorts[1], "/test/remote");
            config.registerPort(_senderPorts[2], "/test/same");
            config.registerPort(_senderPorts[3], "/test/different0");
        }

        void send(size_t idx)
        {
            _senderPorts[idx].setValue(TestValue(idx));
        }

    private:
        std::vector<SenderPort<TestValue>> _senderPorts;
    };


    class TestReceiver : public Component {
    public:
        TestReceiver() :
            Component("TestReceiver")
        {}

        void configure(IComponentConfig& config) {
            _receiverPorts.emplace_back(*this, "local");
            _receiverPorts.emplace_back(*this, "remote");
            _receiverPorts.emplace_back(*this, "same");
            _receiverPorts.emplace_back(*this, "different");

            config.registerPort(_receiverPorts[0], "/test/local");
            config.registerPort(_receiverPorts[1], "/test/remote");
            config.registerPort(_receiverPorts[2], "/test/same");
            config.registerPort(_receiverPorts[3], "/test/different2");
        }

        bool received(size_t idx)
        {
            if(!_receiverPorts[idx].hasValue()) return false;
            auto value = _receiverPorts[idx].getValue();
            return value->val == idx;
        }

    private:
        std::vector<ReceiverPort<TestValue>> _receiverPorts;
    };

    template<typename T>
    inline void registerValueTypes(T& r) {
        r.template registerType<TestValue>("TestValue");
    }
};


TEST_F(RemoteServiceConfiguratorTest, Simple) {
    mcf::ValueStore vs1;
    mcf::ValueStore vs2;

    registerValueTypes(vs1);
    registerValueTypes(vs2);

    mcf::ComponentManager cm1(vs1);
    mcf::ComponentManager cm2(vs2);

    auto testSender = std::make_shared<TestSender>();
    auto testReceiver = std::make_shared<TestReceiver>();

    cm1.registerComponent(testSender);
    cm2.registerComponent(testReceiver);

    mcf::remote::RemoteServiceConfigurator rsc1(vs1);
    mcf::remote::RemoteServiceConfigurator rsc2(vs2);

    std::string strJson1(
        "{"
        "    \"bridge\": {"
        "        \"sendConnection\": \"tcp://127.0.0.1:5550\","
        "        \"receiveConnection\": \"tcp://127.0.0.1:5551\","
        "        \"sendRules\": ["
        "        {\"topic_local\": \"/test/local\"},"
        "        {\"topic_remote\": \"/test/remote\"},"
        "        {\"topic_local\": \"/test/same\", \"topic_remote\": \"/test/same\"},"
        "        {\"topic_local\": \"/test/different0\", \"topic_remote\": \"/test/different1\"}"
        "        ],"
        "        \"receiveRules\": ["
        "        ]"
        "    }"
        "}"
    );
    Json::Value config1;
    Json::Reader reader;
    reader.parse( strJson1.c_str(), config1 );

    std::string strJson2(
        "{"
        "    \"bridge\": {"
        "        \"sendConnection\": \"tcp://127.0.0.1:5551\","
        "        \"receiveConnection\": \"tcp://127.0.0.1:5550\","
        "        \"sendRules\": ["
        "        ],"
        "        \"receiveRules\": ["
        "        {\"topic_local\": \"/test/local\"},"
        "        {\"topic_remote\": \"/test/remote\"},"
        "        {\"topic_local\": \"/test/same\", \"topic_remote\": \"/test/same\"},"
        "        {\"topic_local\": \"/test/different2\", \"topic_remote\": \"/test/different1\"}"
        "        ]"
        "    }"
        "}"
    );
    Json::Value config2;
    reader.parse( strJson2.c_str(), config2 );

    auto instances1 = rsc1.configureFromJSONNode(config1);
    for (auto& item: instances1)
    {
        cm1.registerComponent(item.second, "com/esrlabs/mcf/remote/RemoteService", item.first);
    }
    auto instances2 = rsc2.configureFromJSONNode(config2);
    for (auto& item: instances2)
    {
        cm2.registerComponent(item.second, "com/esrlabs/mcf/remote/RemoteService", item.first);
    }

    cm1.configure();
    cm2.configure();

    cm1.startup();
    cm2.startup();

    // wait for components to start up
    for(int i = 0; i < 100; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if(testReceiver->getState() == IComponent::StateType::RUNNING) break;
    }

    EXPECT_EQ(testSender->getState(), IComponent::StateType::RUNNING);
    EXPECT_EQ(testReceiver->getState(), IComponent::StateType::RUNNING);

    testSender->send(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    testSender->send(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    testSender->send(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    testSender->send(3);

    // wait for values to arrive
    for(int i = 0; i < 100; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if(testReceiver->received(0) &&
            testReceiver->received(1) &&
            testReceiver->received(2) &&
            testReceiver->received(3)) break;
    }

    EXPECT_TRUE(testReceiver->received(0));
    EXPECT_TRUE(testReceiver->received(1));
    EXPECT_TRUE(testReceiver->received(2));
    EXPECT_TRUE(testReceiver->received(3));

    cm1.shutdown();
    cm2.shutdown();
}

} // end namespace remote

} // end namespace mcf

