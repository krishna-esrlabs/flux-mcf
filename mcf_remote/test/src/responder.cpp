/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_remote/ShmemKeeper.h"
#include "mcf_remote/ShmemClient.h"
#include "mcf_remote/RemoteService.h"
#include "mcf_remote/RemoteServiceUtils.h"

#include "mcf_core/ValueRecorder.h"
#include "mcf_core/ComponentTraceController.h"
#include "mcf_core/ExtMemValue.h"
#include "mcf_remote_test_value_types/McfRemoteTestValueTypes.h"

#include "CLI/CLI.hpp"

#define BOOST_DATE_TIME_NO_LIB
#include <boost/interprocess/managed_shared_memory.hpp>
namespace bip = boost::interprocess;

#include <atomic>
#include <csignal>

namespace {

class RespondingComponent : public mcf::Component
{

public:
    RespondingComponent() :
        mcf::Component("RespondingComponent"),
        _imagePort(*this, "Image"),
        _pointPort(*this, "Point", 1, true),
        _constPointPort(*this, "ConstPoint"),
        _responsePort(*this, "Response")
    {}

    void configure(mcf::IComponentConfig& config)
    {
        config.registerPort(_imagePort, "/generator/image");
        config.registerPort(_pointPort, "/generator/point");
        config.registerPort(_constPointPort, "/generator/constPoint");
        config.registerPort(_responsePort, "/responder/response");
        _imagePort.registerHandler(std::bind(&RespondingComponent::listenImage, this));
        _pointPort.registerHandler(std::bind(&RespondingComponent::listenPoint, this));
        _constPointPort.registerHandler(std::bind(&RespondingComponent::listenConstPoint, this));
    }

    void listenImage()
    {
        if(!_imagePort.hasValue())
        {
            log(mcf::LogSeverity::err, fmt::format("No data on imagePort"));
            exit(-1);
        }

        std::shared_ptr<const values::mcf_remote_test_value_types::mcf_remote_test::TestImageUint8> value = _imagePort.getValue();

        if(!value->id())
        {
            log(mcf::LogSeverity::err, "Invalid id on image");
            exit(-1);
        }

        uint8_t* extMemPtr = (uint8_t*)value->extMemPtr();
        size_t sum = 0;
        for(size_t i = 0; i < value->extMemSize(); ++i) sum+=(size_t)(extMemPtr[i]);

        if(sum != 0u && sum != value->height * value->width * 255 && sum != value->height * value->width * 170)
        {
            log(mcf::LogSeverity::err, fmt::format("Image data corruption detected {}: {} {} {}",
                    value->id(),
                    sum,
                    (size_t)extMemPtr[0],
                    (size_t)extMemPtr[value->extMemSize()-1]));
        }

        respond(value->width);
    }

    void listenPoint()
    {
        while(_pointPort.hasValue())
        {
            std::shared_ptr<const values::mcf_remote_test_value_types::mcf_remote_test::TestPointXYZ> value = _pointPort.getValue();

            uint64_t id = value->id() & 0xffffffff; // strip pid from id to compare with counter in value

            if(1.0f != value->x || -1.0f != value->y || id != value->z)
            {
                log(mcf::LogSeverity::err, fmt::format("Point data corruption detected {}: {} {} {}",
                        value->id(),
                        value->x,
                        value->y,
                        value->z));
               exit(-1);
            }

            respond(value->x);
        }
    }


    void listenConstPoint()
    {
        if(!_constPointPort.hasValue())
        {
            log(mcf::LogSeverity::err, fmt::format("No data on constPointPort"));
            exit(-1);
        }

        std::shared_ptr<const values::mcf_remote_test_value_types::mcf_remote_test::TestPointXYZ> value = _constPointPort.getValue();

        uint64_t id = value->id() & 0xffffffff; // strip pid from id to compare with counter in value

        if(77.0f != value->x || -42.0f != value->y || id != value->z)
        {
            log(mcf::LogSeverity::err, fmt::format("Point data corruption detected {}: {} {} {}",
                    value->id(),
                    value->x,
                    value->y,
                    value->z));
            exit(-1);
        }
    }

    void respond(const int mode)
    {
        // respond with an updated PathPlaningState
        values::mcf_remote_test_value_types::mcf_remote_test::TestInt state(mode);

        _responsePort.setValue(state);
    }

private:

    mcf::ReceiverPort<values::mcf_remote_test_value_types::mcf_remote_test::TestImageUint8> _imagePort;
    mcf::QueuedReceiverPort<values::mcf_remote_test_value_types::mcf_remote_test::TestPointXYZ> _pointPort;
    mcf::ReceiverPort<values::mcf_remote_test_value_types::mcf_remote_test::TestPointXYZ> _constPointPort;
    mcf::SenderPort<values::mcf_remote_test_value_types::mcf_remote_test::TestInt> _responsePort;
};

}

std::atomic_bool run(true);

void
signalHandler(int signal)
{
    run = false;
}

int main(int argc, char* argv[]) {
    // setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::string connectionSend("ipc:///tmp/1");
    std::string connectionRec("ipc:///tmp/0");

    CLI::App app("Performance Test");

    app.add_option("--connectionSend", connectionSend, "Connection of the RemoteService to be used for sending");
    app.add_option("--connectionRec", connectionRec, "Connection of the RemoteService to be used for receiving");

    CLI11_PARSE(app, argc, argv);

    mcf::ValueStore vs;

    values::mcf_remote_test_value_types::registerMcfRemoteTestValueTypes(vs);

    // create ValueRecorder to create a file with sent/received values for post-mortem analysis
    mcf::ValueRecorder traceValueRecorder(vs);
    std::unique_ptr<mcf::ComponentTraceController> componentTraceController;
    traceValueRecorder.start("responderTrace.bin");

    // create ComponentManager
    mcf::ComponentManager cm(vs, "", componentTraceController.get());

    // create a RemoteService supporting shared memory transfers of ExtMemValues
    // shared memory is only used if the shm protocoll is specified in the arguments
    std::shared_ptr<mcf::remote::ShmemKeeper> shmemKeeper(new mcf::remote::SingleFileShmem());
    std::shared_ptr<mcf::remote::ShmemClient> shmemClient(new mcf::remote::ShmemClient());
    auto rs = mcf::remote::buildZmqRemoteService(connectionSend, connectionRec, vs, shmemKeeper, shmemClient);

    // create component that shall receive and check the values over RemoteService and
    // reply to them with another value
    auto rc = std::make_shared<RespondingComponent>();

    // set up system
    cm.registerComponent(rs);
    cm.registerComponent(rc);

    rc->ctrlSetLogLevels(mcf::LogSeverity::trace, mcf::LogSeverity::warn);

    rs->addReceiveRule("/generator/image", "/generator/image");
    rs->addReceiveRule("/generator/point", "/generator/pointRemote");
    rs->addReceiveRule("/generator/constPoint");
    rs->addSendRule("/responder/response", 0, true);

    cm.configure();
    cm.startup();

    // loop until a signal is caught
    while(run)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    cm.shutdown();

    // wait for value recorder to write all data
    while(!traceValueRecorder.writeQueueEmpty())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}

