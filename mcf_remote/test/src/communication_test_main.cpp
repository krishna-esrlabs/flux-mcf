/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_remote/RemoteControl.h"
#include "mcf_core/ReplayEventController.h"
#include "mcf_core/Component.h"
#include "mcf_remote_test_value_types/McfRemoteTestValueTypes.h"
#include "mcf_core/ErrorMacros.h"

#include "CLI/CLI.hpp"

#include <chrono>
#include <thread>
#include <iostream>
#include <signal.h>
#include <limits>

namespace test_values = values::mcf_remote_test_value_types::mcf_remote_test;

std::atomic<bool> runFlag(true);

void sig_int_handler(int sig)
{
    runFlag = false;
}

// Component accepting a position on one port and sening the received position on another port
class PositionRepeater : public mcf::Component {

public:
    PositionRepeater() :
        Component("PositionRepeater"),
        fPointSender(*this, "PointSender"),
        fPointReceiver(*this, "PointReceiver")
    {
    }

    void configure(mcf::IComponentConfig& config) override 
    {
        config.registerPort(fPointSender, "/position");
        config.registerPort(fPointReceiver, "/target");
        registerTriggerHandler([this] { control(); });
    }

    void startup() override 
    {
        trigger();
    }

private:
    void move()
    {
        if(fTarget == nullptr) return;

        // float dx = std::max(-1.0f, std::min(1.0f, fTarget->x - fPosition.x));
        // float dy = std::max(-1.0f, std::min(1.0f, fTarget->y - fPosition.y));
        // fPosition.x += dx;
        // fPosition.y += dy;
        fPosition = *fTarget;
    }

    void control() 
    {
         if(fPointReceiver.hasValue())
         {
             fTarget = fPointReceiver.getValue();
         }

         move();

         fPointSender.setValue(fPosition);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        trigger();
    }

    mcf::SenderPort<test_values::TestPointXY> fPointSender;
    mcf::ReceiverPort<test_values::TestPointXY> fPointReceiver;
    std::shared_ptr<const test_values::TestPointXY> fTarget;
    test_values::TestPointXY fPosition = {0.0f, 0.0f};
};

// Component providing one randomized image on a port
class Camera : public mcf::Component {
public:
    Camera(uint32_t imageWidth) :
        Component("Camera"),
        fImageSender(*this, "ImageSender"),
        fImageWidth(imageWidth)
    {
        if(fImageWidth > std::numeric_limits<uint16_t>::max())
        {
            MCF_ERROR(
                "Requested image size ({}) is bigger than the maximal supported size ({})",
                fImageWidth,
                std::numeric_limits<uint16_t>::max());
            MCF_ASSERT(false, "Invalid image size")
        }
    }

    void configure(mcf::IComponentConfig& config) override
    {
        config.registerPort(fImageSender, "/image");
        registerTriggerHandler([this] { outputImage(); });
    }

    void startup() override 
    {
        trigger();
    }

private:
    void outputImage() {
        test_values::TestImageUint8 image;
        image.width = fImageWidth;  ///< Width of image
        image.height = 1024;  ///< Height of image
        image.pitch = fImageWidth;  ///< Pitch of image
        image.timestamp = 848649600000ul;  ///< Timestamp of image (microseconds)
        image.extMemInit((uint64_t)image.pitch * image.height);
        fImageSender.setValue(std::move(image));
    }

    mcf::SenderPort<test_values::TestImageUint8> fImageSender;
    uint32_t fImageWidth;
};

int main(int argc, char **argv) {

    CLI::App app("MCF Remote Control Test Engine");

    int port = 6666;
    app.add_option("--port", port, "The port to be used");
    uint32_t imageWidth = 1024;
    app.add_option("--size", imageWidth, "Size of the image to be transferred in KB");

    CLI11_PARSE(app, argc, argv)

    mcf::ValueStore valueStore;
    values::mcf_remote_test_value_types::registerMcfRemoteTestValueTypes(valueStore);

    mcf::ComponentManager componentManager(valueStore, "./");

    // create and register test components
    auto pc = std::make_shared<PositionRepeater>();
    auto cam = std::make_shared<Camera>(imageWidth);
    componentManager.registerComponent(pc);
    componentManager.registerComponent(cam);

    // create and register remote control
    auto remoteControl = std::make_shared<mcf::remote::RemoteControl>(
        port,
        componentManager,
        valueStore);

    // create and register the dummy event source
    componentManager.registerComponent(remoteControl);

    componentManager.configure();
    componentManager.startup();

    // wait until interrupted
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sig_int_handler;    sigaction(SIGHUP, &action, NULL);  // controlling terminal closed, Ctrl-D

    sigaction(SIGINT, &action, NULL);  // Ctrl-C
    sigaction(SIGQUIT, &action, NULL); // Ctrl-\, clean quit with core dump
    sigaction(SIGABRT, &action, NULL); // abort() called.
    sigaction(SIGTERM, &action, NULL); // kill command
    sigaction(SIGSTOP, &action, NULL); // kill command
    sigaction(SIGUSR1, &action, NULL); // kill command

    while (runFlag)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    componentManager.shutdown();

    return 0;
}
