/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/Mcf.h"
#include "mcf_core/PluginInterface.h"
#include "test/TestValue.h"

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
        fTackPort.setValue(mcf::test::TestValue(val));
    }

    std::vector<std::string> events;
    mcf::ReceiverPort<mcf::test::TestValue> fTickPort;
    mcf::SenderPort<mcf::test::TestValue> fTackPort;
};

mcf::Plugin
initializePlugin()
{
    return mcf::PluginBuilder("SimplePluginLibrary")
        .addComponentType<TestComponent>("com/esrlabs/TestComponent")
        .toPlugin();
}