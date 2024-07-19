/*
 * Copyright (c) 2024 Accenture
 */

#include "CLI/CLI.hpp"
#include "mcf_core/ErrorMacros.h"
#include "json/json.h"
#include "mcf_core/Mcf.h"
#include "mcf_remote/RemoteControl.h"
#include "mcf_remote/RemoteService.h"
#include "mcf_remote/RemoteServiceConfigurator.h"
#include "value_types/ValueTypes.h"

#include <chrono>
#include <csignal>
#include <cstring>
#include <memory>
#include <thread>


namespace {

std::atomic<bool> runFlag(true);

void sig_int_handler(int)
{
    runFlag = false;
}

// remote control port
constexpr int REMOTE_CONTROL_PORT = 6666;

// Remote service type name
constexpr const char *REMOTE_SERVICE_TYPE_NAME = "com/esrlabs/mcf/remote/RemoteService";


// Flux topics
constexpr const char *SEND_TOPIC_1 = "/topic_to_py_1";
constexpr const char *SEND_TOPIC_2 = "/topic_to_py_2";
constexpr const char *SEND_TOPIC_3 = "/topic_to_py_3";
constexpr const char *RECEIVE_TOPIC_1 = "/topic_from_py_1";
constexpr const char *RECEIVE_TOPIC_2 = "/topic_from_py_2";
constexpr const char *RECEIVE_TOPIC_3 = "/topic_from_py_3";

// remote service configuration as
constexpr const char *REMOTE_SERVICE_CONFIG =
        R"json({
        "RemoteServices": {
          "python_bridge": {
            "doc": "Bridge to python",
            "sendConnection": "tcp://127.0.0.1:5561",
            "receiveConnection": "tcp://127.0.0.1:5560",
            "sendRules": [
                { "topic_local": "/topic_to_py_1", "blocking": false, "queue_length": 1 },
                { "topic_local": "/topic_to_py_2", "blocking": false, "queue_length": 1 },
                { "topic_local": "/topic_to_py_3", "blocking": false, "queue_length": 1 }
            ],
            "receiveRules": [
              { "topic_remote": "/topic_from_py_1" },
              { "topic_remote": "/topic_from_py_2" },
              { "topic_remote": "/topic_from_py_3" }
            ]
          }
        }
      })json";

template<typename T>
class EchoComponent : public mcf::Component
{
public:
    EchoComponent(std::string inTopic, std::string outTopic)
    : Component("EchoComponent")
    , fOutPort(*this, "out")
    , fInPort(*this, "in", 1)
    , fInTopic(std::move(inTopic))
    , fOutTopic(std::move(outTopic))
    {}

    void configure(mcf::IComponentConfig& config) override
    {
        config.registerPort(fOutPort, fOutTopic);
        config.registerPort(fInPort, fInTopic);
        fInPort.registerHandler([this] { onInput(); });
    }

private:

    void onInput()
    {
        while (fInPort.hasValue())
        {
            auto value = fInPort.getValue();
            if (!value)
            {
                MCF_WARN_NOFILELINE("Received empty value, skipping");
                continue;
            }
            MCF_INFO_NOFILELINE("Received value, echoing back");
            fOutPort.setValue(value);
        }
    };

    mcf::SenderPort<T> fOutPort;
    mcf::QueuedReceiverPort<T> fInPort;

    std::string fInTopic;
    std::string fOutTopic;

};

void configureComponents(mcf::ComponentManager &componentManager)
{
    auto ec1 = std::make_shared<EchoComponent<values::value_types::base::PointXYZ>>(RECEIVE_TOPIC_1, SEND_TOPIC_1);
    auto ec2 = std::make_shared<EchoComponent<values::value_types::camera::ImageUint8>>(RECEIVE_TOPIC_2, SEND_TOPIC_2);
    auto ec3 = std::make_shared<EchoComponent<values::value_types::base::MultiExtrinsics>>(RECEIVE_TOPIC_3, SEND_TOPIC_3);
    componentManager.registerComponent(ec1, "/component/1/type/name", "echo_comp_1");
    componentManager.registerComponent(ec2, "/component/2/type/name", "echo_comp_2");
    componentManager.registerComponent(ec3, "/component/3/type/name", "echo_comp_3");
}

// helper method to configure remote service instances
void configureRemoteServices(const char* remoteServiceConfigString,
                             mcf::ValueStore& valueStore,
                             mcf::ComponentManager& componentManager)
{
    // read json value from hard-coded config string
    Json::Reader reader;
    Json::Value configValue;
    bool isOk = reader.parse(remoteServiceConfigString, configValue);
    MCF_ASSERT(isOk, "Failed to parse remote service config");
    const Json::Value remoteServiceConfig = configValue["RemoteServices"];

    // use RemoteServiceConfigurator to create remote service instances as needed
    mcf::remote::RemoteServiceConfigurator remoteServiceConfigurator(valueStore);
    auto instances = remoteServiceConfigurator.configureFromJSONNode(remoteServiceConfig);

    // register instances
    for (auto& item: instances)
    {
        componentManager.registerComponent(item.second, REMOTE_SERVICE_TYPE_NAME, item.first);
    }
}


} // anonymous namespace

int main(int argc, char **argv)
{
    // create value store for component communication
    mcf::ValueStore valueStore;
    values::value_types::registerValueTypes(valueStore);

    struct sigaction action {};
    memset(&action, 0, sizeof(action));
    action.sa_handler = sig_int_handler;

    sigaction(SIGHUP, &action, nullptr);  // controlling terminal closed, Ctrl-D
    sigaction(SIGINT, &action, nullptr);  // Ctrl-C
    sigaction(SIGQUIT, &action, nullptr); // Ctrl-\, clean quit with core dump
    sigaction(SIGABRT, &action, nullptr); // abort() called.
    sigaction(SIGTERM, &action, nullptr); // kill command
    sigaction(SIGSTOP, &action, nullptr); // kill command
    sigaction(SIGUSR1, &action, nullptr); // kill command

    mcf::ComponentManager componentManager(valueStore, "./", nullptr);

    auto remoteControl = std::make_shared<mcf::remote::RemoteControl>(REMOTE_CONTROL_PORT, componentManager, valueStore);
    componentManager.registerComponent(remoteControl);

    configureComponents(componentManager);
    configureRemoteServices(REMOTE_SERVICE_CONFIG, valueStore, componentManager);

    componentManager.configure();
    componentManager.startup();

    while (runFlag)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    componentManager.shutdown();

    return 0;
}

