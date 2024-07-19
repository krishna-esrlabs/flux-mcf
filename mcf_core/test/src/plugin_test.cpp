/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_core/ExtMemValue.h"
#include "mcf_core/Mcf.h"
#include "mcf_core/PluginLoader.h"
#include "mcf_core/PluginManager.h"
#include "test/TestValue.h"

namespace
{
class PluginTest : public ::testing::Test
{
public:

    #ifdef COMPILED_WITH_CMAKE
        static constexpr const char* PLUGIN_DSO_PATH = "libplugin_dso.so";
    #else
        static constexpr const char* PLUGIN_DSO_PATH = "build/TestPluginDSO_mcf_core_UnitTestBase/plugin_dso.so";
    #endif

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

    class TestComponentA : public mcf::Component
    {
    public:
        TestComponentA()
        : mcf::Component("TestComponentA"), fTickPort(*this, "Tick"), fTackPort(*this, "Tack")
        {
            fTickPort.registerHandler(std::bind(&TestComponentA::tick, this));
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

    class TestComponent2 : public mcf::Component
    {
    public:
        TestComponent2()
        : mcf::Component("TestComponent2"), fTickPort(*this, "Tick"), fTackPort(*this, "Tack")
        {
        }

        void configure(mcf::IComponentConfig& config)
        {
            config.registerPort(fTickPort, "/tick");
            config.registerPort(fTackPort, "/tack");
        }

        mcf::SenderPort<TestValue> fTickPort;
        mcf::SenderPort<TestValue> fTackPort;
    };

    class TestComponent3 : public mcf::Component
    {
    public:
        TestComponent3() : mcf::Component("TestComponent3"), fExtMemPort(*this, "ExtMem") {}

        void configure(mcf::IComponentConfig& config)
        {
            config.registerPort(fExtMemPort, "/extmem");
        }

        void startup()
        {
            fExtMemPort.setValue(TestValueExtMem(5));
            TestValueExtMem tv2(7);
            fExtMemPort.setValue(std::move(tv2));
        }

        mcf::SenderPort<TestValueExtMem> fExtMemPort;
    };

    class TestValueExtMem : public mcf::ExtMemValue<uint8_t>
    {
    public:
        TestValueExtMem(int val = 0) : val(val){};
        int val;
        MSGPACK_DEFINE(val);
    };

    mcf::Plugin simplePlugin()
    {
        return mcf::PluginBuilder("simple")
            .addComponentType<TestComponent>("com/esrlabs/Test")
            .addComponentType<TestComponent2>("com/esrlabs/Test2")
            .addComponentType<TestComponent3>("com/esrlabs/Test3")
            .toPlugin();
    }
};

TEST_F(PluginTest, LoadPlugin)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);
    mcf::PluginManager pluginManager(instantiator);

    auto types = pluginManager.registerPlugin(simplePlugin());

    auto loadedPlugins = pluginManager.getPlugins();
    EXPECT_EQ(loadedPlugins, std::vector<std::string>({"simple"}));

    for (const auto& type : types)
    {
        auto proxy = instantiator.createComponent(type, type);
        proxy.configure();
        proxy.startup();
    }

    while (!valueStore.hasValue("/extmem"))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    valueStore.setValue("/tick", TestValue(23));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(valueStore.hasValue("/tack"));

    auto ptr = valueStore.getValue<TestValueExtMem>("/extmem");
    EXPECT_EQ(7, ptr->val);

    manager.shutdown();
}

TEST_F(PluginTest, LoadAndUnloadPlugin)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);
    mcf::PluginManager pluginManager(instantiator);

    auto types = pluginManager.registerPlugin(simplePlugin());

    for (const auto& type : types)
    {
        auto proxy = instantiator.createComponent(type, type);
        proxy.configure();
        proxy.startup();
    }

    EXPECT_NE(manager.getComponents().size(), 0);

    pluginManager.erasePlugin("simple");

    auto components = manager.getComponents();
    EXPECT_EQ(components.size(), 0);

    valueStore.setValue("/tick", TestValue(23));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_FALSE(valueStore.hasValue("/tack"));

    manager.shutdown();
}

TEST_F(PluginTest, LoadAndUnloadPluginWithExistingComponent)
{
    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);
    mcf::PluginManager pluginManager(instantiator);

    auto component = std::make_shared<TestComponentA>();
    manager.registerComponent(component);

    manager.configure();
    manager.startup();

    auto types = pluginManager.registerPlugin(simplePlugin());

    for (const auto& type : types)
    {
        auto proxy = instantiator.createComponent(type, type);
        proxy.configure();
        proxy.startup();
    }

    EXPECT_NE(manager.getComponents().size(), 0);

    pluginManager.erasePlugin("simple");

    auto components = manager.getComponents();
    EXPECT_EQ(components.size(), 1);

    valueStore.setValue("/tick", TestValue(23));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_TRUE(valueStore.hasValue("/tack"));
    auto ptr = valueStore.getValue<TestValue>("/tack");
    EXPECT_EQ(23, ptr->val);

    manager.shutdown();
}

TEST_F(PluginTest, LoadPluginFromDSO)
{
    mcf::PluginLoader loader;
    auto plugin = loader.load(PluginTest::PLUGIN_DSO_PATH);
    EXPECT_EQ(plugin.name(), "SimplePluginLibrary");

    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);
    mcf::PluginManager pluginManager(instantiator);

    auto types = pluginManager.registerPlugin(plugin);

    for (const auto& type : types)
    {
        auto proxy = instantiator.createComponent(type, type);
        proxy.configure();
        proxy.startup();
    }

    valueStore.setValue("/tick", TestValue(23));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_TRUE(valueStore.hasValue("/tack"));

    auto ptr = valueStore.getValue<TestValue>("/tack");
    EXPECT_EQ(23, ptr->val);

    manager.shutdown();
}

TEST_F(PluginTest, ReLoadPluginFromDSO)
{
    mcf::PluginLoader loader;
    auto plugin = loader.load(PluginTest::PLUGIN_DSO_PATH);
    EXPECT_EQ(plugin.name(), "SimplePluginLibrary");

    mcf::ValueStore valueStore;
    mcf::ComponentManager manager(valueStore);
    mcf::ComponentInstantiator instantiator(manager);
    mcf::PluginManager pluginManager(instantiator);

    auto types = pluginManager.registerPlugin(plugin);

    for (const auto& type : types)
    {
        instantiator.createComponent(type, type);
    }

    manager.configure();
    manager.startup();

    valueStore.setValue("/tick", TestValue(23));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_TRUE(valueStore.hasValue("/tack"));

    auto ptr = valueStore.getValue<TestValue>("/tack");
    EXPECT_EQ(23, ptr->val);

    // unload the plugin, reload it from disk
    // first, retrieve the component's id
    auto id = manager.getComponents().at(0).id();

    pluginManager.erasePlugin(plugin.name());

    EXPECT_EQ(pluginManager.getPlugins().size(), 0);
    EXPECT_EQ(manager.getComponents().size(), 0);
    EXPECT_EQ(instantiator.listComponents().size(), 0);
    EXPECT_EQ(instantiator.listComponentTypes().size(), 0);
    EXPECT_THROW(pluginManager.getComponentTypes(plugin.name()).size(), std::runtime_error);

    valueStore.setValue("/tick", TestValue(17));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    ptr = valueStore.getValue<TestValue>("/tack");
    EXPECT_EQ(23, ptr->val); // expect old value

    auto secondPlugin   = loader.load(PluginTest::PLUGIN_DSO_PATH);
    auto qualifiedTypes = pluginManager.registerPlugin(plugin);
    for (const auto& t : qualifiedTypes)
    {
        auto proxy = instantiator.createComponent(t, t);
        manager.configure(proxy);
        manager.startup(proxy);
        EXPECT_NE(proxy.id(), id);
    }

    valueStore.setValue("/tick", TestValue(42));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    ptr = valueStore.getValue<TestValue>("/tack");
    EXPECT_EQ(42, ptr->val); // expect new value

    manager.shutdown();
}

} // namespace