/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_core/Mcf.h"

namespace mcf {

class ConfigCheckTest : public ::testing::Test {
public:
    class TestValue : public mcf::Value {
    public:
        int val;
        MSGPACK_DEFINE(val);
    };

    class TestValue2 : public mcf::Value {
    public:
        int val;
        MSGPACK_DEFINE(val);
    };

    class TestValueSender : public Component {
    public:
        TestValueSender() :
            Component("TestValueSender"),
            fPort(*this, "Port")
        {}
        void configure(IComponentConfig& config) {
            config.registerPort(fPort, "/test1");
        }
        SenderPort<TestValue> fPort;
    };

    class TestValueSender2 : public Component {
    public:
        TestValueSender2() :
            Component("TestValueSender2"),
            fPort(*this, "Port")
        {}
        void configure(IComponentConfig& config) {
            config.registerPort(fPort, "/test1");
        }
        SenderPort<TestValue2> fPort;
    };

    class TestValueReceiver : public Component {
    public:
        TestValueReceiver() :
            Component("TestValueReceiver"),
            fPort(*this, "Port")
        {}
        void configure(IComponentConfig& config) {
            config.registerPort(fPort, "/test1");
        }
        ReceiverPort<TestValue> fPort;
    };

};

TEST_F(ConfigCheckTest, OneToOne) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto sender = std::make_shared<TestValueSender>();
    auto receiver = std::make_shared<TestValueReceiver>();

    manager.registerComponent(sender);
    manager.registerComponent(receiver);

    ASSERT_TRUE(manager.configure());
}

TEST_F(ConfigCheckTest, OneToMany) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto sender = std::make_shared<TestValueSender>();
    auto receiver = std::make_shared<TestValueReceiver>();
    auto receiver2 = std::make_shared<TestValueReceiver>();

    manager.registerComponent(sender);
    manager.registerComponent(receiver);
    manager.registerComponent(receiver2);

    ASSERT_TRUE(manager.configure());
}

TEST_F(ConfigCheckTest, ManyToOne) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto sender = std::make_shared<TestValueSender>();
    auto sender2 = std::make_shared<TestValueSender>();
    auto receiver = std::make_shared<TestValueReceiver>();

    manager.registerComponent(sender);
    manager.registerComponent(sender2);
    manager.registerComponent(receiver);

    ASSERT_TRUE(manager.configure());
}

TEST_F(ConfigCheckTest, ManyToMany) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto sender = std::make_shared<TestValueSender>();
    auto sender2 = std::make_shared<TestValueSender>();
    auto receiver = std::make_shared<TestValueReceiver>();
    auto receiver2 = std::make_shared<TestValueReceiver>();

    manager.registerComponent(sender);
    manager.registerComponent(sender2);
    manager.registerComponent(receiver);
    manager.registerComponent(receiver2);

    ASSERT_TRUE(manager.configure());
}

TEST_F(ConfigCheckTest, NoReceiver) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto sender = std::make_shared<TestValueSender>();

    manager.registerComponent(sender);

    ASSERT_TRUE(manager.configure());
}

TEST_F(ConfigCheckTest, NoSender) {
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);

    auto receiver = std::make_shared<TestValueReceiver>();

    manager.registerComponent(receiver);

    ASSERT_TRUE(manager.configure());
}

// Temporarily disabled, because config check is disabled
//TEST_F(ConfigCheckTest, WrongTypesForbidden) {
//    mcf::ValueStore valueStore;
//    mcf::ComponentManager manager(valueStore);
//
//    auto sender = std::make_shared<TestValueSender2>();
//    auto receiver = std::make_shared<TestValueReceiver>();
//
//    manager.registerComponent(sender);
//    manager.registerComponent(receiver);
//
//    ASSERT_FALSE(manager.configure());
//}

}
