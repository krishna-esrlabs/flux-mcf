/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_remote/RemoteService.h"
#include "mcf_remote/ShmemKeeper.h"
#include "mcf_remote/ShmemClient.h"
#include "mcf_remote/RemoteServiceUtils.h"

#include "mcf_core/ValueRecorder.h"
#include "mcf_core/ComponentTraceController.h"
#include "mcf_core/CountingIdGenerator.h"
#include "mcf_remote_test_value_types/McfRemoteTestValueTypes.h"

#include "CLI/CLI.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>

#include <random>

std::atomic_bool run(true);

void
signalHandler(int signal)
{
    run = false;
}

namespace {

class GeneratingComponent : public mcf::Component
{

public:
    GeneratingComponent(size_t size, uint64_t periodLength=33) :
        mcf::Component("GeneratingComponent"),
        _imagePort(*this, "Image"),
        _pointPort(*this, "Point"),
        _constPointPort(*this, "ConstPoint"),
        _responsePort(*this, "Response", 10, true),
        _size(size),
        _periodLength(periodLength)
    {}

    ~GeneratingComponent()
    { }

    void configure(mcf::IComponentConfig& config)
    {
        config.registerPort(_imagePort, "/generator/image");
        config.registerPort(_pointPort, "/generator/point");
        config.registerPort(_constPointPort, "/generator/constPoint");
        config.registerPort(_responsePort, "/responder/response");
        _responsePort.registerHandler(std::bind(&GeneratingComponent::listenState, this));
    }

    void startSending(size_t iterations)
    {
        uint64_t cnt = 0;
        _start = std::chrono::high_resolution_clock::now();

        sendPoint(++cnt, _constPointPort, 77.0f, -42.0f);

        while(cnt < iterations && run)
        {
            std::chrono::high_resolution_clock::time_point frametime = std::chrono::high_resolution_clock::now();

            sendImage(++cnt, _size, _size, _imagePort);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            sendPoint(++cnt, _pointPort);

            // busy wait until period end to simulate some load
            while(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - frametime).count() < _periodLength)
            {}
        }
    }

private:
    void sendImage(uint64_t stamp, size_t width, size_t height, mcf::SenderPort<values::mcf_remote_test_value_types::mcf_remote_test::TestImageUint8>& port)
    {
        values::mcf_remote_test_value_types::mcf_remote_test::TestImageUint8 image(width, height, width, stamp);
        image.extMemInit(width * height);

        if(stamp % 3 == 0)
        {
            memset(image.extMemPtr(), 0x00, width * height);
        }
        else if(stamp % 3 == 1)
        {
            memset(image.extMemPtr(), 0xff, width * height);
        }
        else
        {
            memset(image.extMemPtr(), 0xaa, width * height);
        }

        auto imageValue = createValue(std::move(image));

        port.setValue(imageValue);
    }

    void sendPoint(uint64_t stamp, mcf::SenderPort<values::mcf_remote_test_value_types::mcf_remote_test::TestPointXYZ>& port, const float x = 1.0f, const float y = -1.0f)
    {
        values::mcf_remote_test_value_types::mcf_remote_test::TestPointXYZ point(x, y, stamp);

        auto pointValue = createValue(std::move(point));

        port.setValue(pointValue);
    }

    void listenState()
    {
        while(_responsePort.hasValue())
        {
            std::shared_ptr<const values::mcf_remote_test_value_types::mcf_remote_test::TestInt> value = _responsePort.getValue();

            if(!value->id())
            {
                log(mcf::LogSeverity::err, "Invalid id on response");
                exit(-1);
            }

            if(value->mode != 1 && value->mode != _size)
            {
                log(mcf::LogSeverity::err, fmt::format("Response has invalid mode: {}", value->mode));
                exit(-1);
            }
        }
    }

    mcf::SenderPort<values::mcf_remote_test_value_types::mcf_remote_test::TestImageUint8> _imagePort;
    mcf::SenderPort<values::mcf_remote_test_value_types::mcf_remote_test::TestPointXYZ> _pointPort;
    mcf::SenderPort<values::mcf_remote_test_value_types::mcf_remote_test::TestPointXYZ> _constPointPort;
    mcf::QueuedReceiverPort<values::mcf_remote_test_value_types::mcf_remote_test::TestInt> _responsePort;

    size_t _size;
    uint64_t _periodLength;
    std::chrono::high_resolution_clock::time_point _start;
};

}

int main(int argc, char* argv[])
{
    // setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    size_t iterations = 1000;
    size_t size = 1024;
    uint64_t periodLength = 33;
    std::string connectionSend("ipc:///tmp/0");
    std::string connectionRec("ipc:///tmp/1");

    CLI::App app("RemoteService Test");

    app.add_option("--n", iterations, "Number of values to be sent");
    app.add_option("--size", size, "Side length of the quadratic image to be transferred");
    app.add_option(
        "--periodLength",
        periodLength,
        "Length of one period (in ms) in which one value on each port is sent");
    app.add_option(
        "--connectionSend",
        connectionSend,
        "Connection of the RemoteService to be used for sending");
    app.add_option(
        "--connectionRec",
        connectionRec,
        "Connection of the RemoteService to be used for receiving");

    CLI11_PARSE(app, argc, argv);

    mcf::ValueStore vs;
    values::mcf_remote_test_value_types::registerMcfRemoteTestValueTypes(vs);

    // create ValueRecorder to create a file with sent/received values for post-mortem analysis
    mcf::ValueRecorder traceValueRecorder(vs);
    std::unique_ptr<mcf::ComponentTraceController> componentTraceController;
    traceValueRecorder.start("generatorTrace.bin");

    // create ComponentManager with custom IdGenerator enabling checks if values were not
    // transferred
    mcf::ComponentManager cm(
        vs,
        "",
        componentTraceController.get(),
        std::make_shared<mcf::CountingIdGenerator>());

    // create a RemoteService supporting shared memory transfers of ExtMemValues
    // shared memory is only used if the shm protocoll is specified in the arguments
    std::shared_ptr<mcf::remote::ShmemKeeper> shmemKeeper(new mcf::remote::SingleFileShmem());
    std::shared_ptr<mcf::remote::ShmemClient> shmemClient(new mcf::remote::ShmemClient());
    auto rs = mcf::remote::buildZmqRemoteService(
        connectionSend,
        connectionRec,
        vs,
        shmemKeeper,
        shmemClient);

    // create component that generates the values which will be transferred by the RemoteService
    auto gc = std::make_shared<GeneratingComponent>(size, periodLength);

    // set up system
    cm.registerComponent(rs);
    cm.registerComponent(gc);

    gc->ctrlSetLogLevels(mcf::LogSeverity::trace, mcf::LogSeverity::critical);
    rs->ctrlSetLogLevels(mcf::LogSeverity::trace, mcf::LogSeverity::critical);

    rs->addSendRule("/generator/image" , 0);
    rs->addSendRule("/generator/point", "/generator/pointRemote", 5, true);
    rs->addSendRule("/generator/constPoint", 1, false);
    rs->addReceiveRule("/responder/response");

    cm.configure();
    cm.startup();

    // start generating and transferring values
    // blocks until the specified number of values has been sent
    gc->startSending(iterations);

    cm.shutdown();

    // wait for value recorder to write all data
    while(!traceValueRecorder.writeQueueEmpty())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}
