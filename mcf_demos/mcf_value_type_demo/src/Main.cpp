/**
 * Main script which sets up MCF infrastructure and runs for a fixed time before exiting.
 *
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/Mcf.h"
#include "mcf_remote/RemoteControl.h"

#include "mcf_example_types/McfExampleTypes.h"
#include "mcf_value_type_demo/PoseReceiverComponent.h"


namespace {

static constexpr const char* INPUT_POSE_TOPIC = "/pose/input";

} // namespace


int main(int argc, char **argv)
{
    // Initialize parts of the middleware
    // The ValueStore is the central message exchange object, similar to a database
    mcf::ValueStore valueStore;

    // The ComponentManager takes care of the Component lifecycle
    mcf::ComponentManager componentManager(valueStore);

    // Instantiate and register components.
    auto poseReceiverComponent = std::make_shared<mcf_value_type_demo::PoseReceiverComponent>();
    auto poseReceiverProxy = componentManager.registerComponent(poseReceiverComponent);

    // Register all the value types from the McfExampleTypes value types package. This tells the 
    // value store how it should serialise / deserialise the value types.
    values::mcf_example_types::registerMcfExampleTypes(valueStore);

    // Register the Remote Control Component which allows values to be sent from a Python script 
    // using the Python Remote Control API.
    auto remoteControl = std::make_shared<mcf::remote::RemoteControl>(6666, componentManager, valueStore);
    componentManager.registerComponent(remoteControl);

    // Set up the wiring between all known Components
    componentManager.configure();

    // Map topics to ports
    poseReceiverProxy.mapPort("in_pose", INPUT_POSE_TOPIC);
    
    // Start the event loop of each registered Component
    componentManager.startup();
    
    // Wait for a message which will be written a Python script and received by the PoseReceiverComponent.
    const std::string inputTopicName = "/pose/input";
    std::this_thread::sleep_for(std::chrono::seconds(1));

    componentManager.shutdown();

    return EXIT_SUCCESS;
}