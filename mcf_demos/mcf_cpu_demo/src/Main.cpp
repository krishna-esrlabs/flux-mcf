/**
 * Main script which gets command line arguments, sets up MCF infrastructure and runs in an infinite
 * loop until user interrupt is received.
 * 
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/Mcf.h"
#include "mcf_cpu_demo/ColourInverterComponent.h"
#include "mcf_core/ValueRecorder.h"
#include "mcf_cpu_demo/ImageFilterComponent.h"
#include "mcf_cpu_demo_value_types/McfCpuDemoValueTypes.h"
#include "mcf_remote/RemoteControl.h"

#include "CLI/CLI.hpp"
#include <chrono>
#include <signal.h>
#include <string.h>


std::atomic<bool> runFlag(true);

void sig_int_handler(int sig)
{
    runFlag = false;
}


namespace {

static constexpr const char* INPUT_IMAGE_TOPIC = "/image/raw";
static constexpr const char* INVERTED_IMAGE_TOPIC = "/image/inverted";
static constexpr const char* FILTER_PARAMS_TOPIC = "/filter/params";
static constexpr const char* BLURRED_IMAGE_TOPIC = "/image/blurred";

} // namespace


int main(int argc, char **argv)
{
    CLI::App app("Mcf Demo Project");

    std::string configDirectory;
    app.add_option("config-dir", configDirectory, "Full directory path containing component configuration files")->required();

    std::string recordingDirectory;
    CLI::Option* recordingDirectoryPtr = app.add_option("--record-dir", recordingDirectory, "Location where config, video, recorded streams, value store data, and events will be saved");

    bool recordValues = false;
    app.add_flag("--record-values", recordValues, "Record value store messages into record.bin")->needs(recordingDirectoryPtr);

    bool recordEvents = false;
    app.add_flag("--record-events", recordEvents, "Record events events.bin")->needs(recordingDirectoryPtr);

    CLI11_PARSE(app, argc, argv);

    // create value store for component communication
    mcf::ValueStore valueStore;
    values::mcf_cpu_demo_value_types::registerMcfCpuDemoValueTypes(valueStore);
    mcf::ValueRecorder valueRecorder(valueStore);

    // create separate value store for component tracing with corresponding value recorder
    mcf::ValueStore traceValueStore;
    mcf::ValueRecorder traceValueRecorder(traceValueStore);

    std::unique_ptr<mcf::ComponentTraceController> componentTraceController;
    if (recordValues)
    {
        valueRecorder.start(recordingDirectory + "/record.bin");
    }

    if (recordEvents)
    {
        traceValueRecorder.start(recordingDirectory + "/trace.bin");
        componentTraceController = std::make_unique<mcf::ComponentTraceController>("S", traceValueStore);
        componentTraceController->enableTrace(true);
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sig_int_handler;

    sigaction(SIGHUP, &action, NULL);  // controlling terminal closed, Ctrl-D
    sigaction(SIGINT, &action, NULL);  // Ctrl-C
    sigaction(SIGQUIT, &action, NULL); // Ctrl-\, clean quit with core dump
    sigaction(SIGABRT, &action, NULL); // abort() called.
    sigaction(SIGTERM, &action, NULL); // kill command
    sigaction(SIGSTOP, &action, NULL); // kill command
    sigaction(SIGUSR1, &action, NULL); // kill command

    mcf::ComponentManager componentManager(valueStore, configDirectory, componentTraceController.get());

    auto remoteControl = std::make_shared<mcf::remote::RemoteControl>(6666, componentManager, valueStore);
    componentManager.registerComponent(remoteControl);

    // Instantiate and register components.
    auto colourInverterComponent = std::make_shared<mcf_cpu_demo::ColourInverterComponent>();
    auto colourInverterProxy = componentManager.registerComponent(colourInverterComponent);

    auto imageBlurringComponent = std::make_shared<mcf_cpu_demo::ImageFilterComponent>();
    auto imageBlurringProxy = componentManager.registerComponent(imageBlurringComponent);

    componentManager.configure();

    // Map topics to ports
    colourInverterProxy.mapPort("in_image", INPUT_IMAGE_TOPIC);
    colourInverterProxy.mapPort("out_inverted_image", INVERTED_IMAGE_TOPIC);
    imageBlurringProxy.mapPort("in_inverted_image", INVERTED_IMAGE_TOPIC);
    imageBlurringProxy.mapPort("in_filter_params", FILTER_PARAMS_TOPIC);
    imageBlurringProxy.mapPort("out_blurred_image", BLURRED_IMAGE_TOPIC);

    componentManager.startup();

    while (runFlag)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    componentManager.shutdown();
    if (recordValues)
    {
        valueRecorder.stop();
    }
    if (recordEvents)
    {
        traceValueRecorder.stop();
    }

    return 0;
}

