MCF - a minimalistic distributed processing framework
=====================================================

#### Generating Documentation (Optional)
We use sphinx to generate our code documentation. To generate the documentation:
1) Install python dependencies: `pip install -r docs/requirements.txt`
2) Generate documentation: `bash generate_docs.sh build`
3) Open documentation: `gnome-open docs/sphinx/index.html`
4) Clean documentation: `bash generate_docs.sh clean`
## Overview

MCF (Messages and Components Framework) is a minimalistic framework to harness
multi-node computations, similar to ROS, Apollo.auto's CyberRT, AUTOSAR
Adaptive and others. The development has been specifically focused on
simplicity, minimal abstraction, and the ability to use in near-embedded and
embedded environments.

## Rationale

Typical applications in automotive and robotics are best tackled with
event-driven, concurrent software architectures. Hence, MCF provides a way to
compose such architectures by defining basic blocks -- `Component`s, which
communicate `Value`s over `Port`s. Each `Component` runs in a separate thread,
which enables different data processing steps to run in parallel.

### Features

* Zero-copy in-process data sharing
* Ability to isolate parts of computation in separate processes
* Reconfiguration of processing graph without recompilation
* Introspection and prototyping from Python
* Real-time thread scheduling support
* Builds and runs on x86_64 and ARM aarch64 platforms

### Why MCF?

In contrast to ROS and CyberRT, we have built MCF to be minimalistic.
Non-goals are automated service discovery, QoS layers, custom/proprietary
build systems and environments, experimental and configurable multi-tasking
mechanisms etc.
Instead, our focus lies in a small, feature-complete, fully controllable
codebase.

## Usage

MCF is meant to be used as a library. To build the library, the following
dependencies (in addition to those of your particular project) are required.

* (core) C++14 compiler (GCC ≥ 7)
* (core) spdlog
* (core) gtest
* (core) zeromq
* (core) msgpack-c
* (core) jsoncpp
* (remote) Boost.Interprocess
* (remote) CLI11
* (cuda) CUDA ≥ 10

Compiling MCF is possible with [CMake](https://cmake.org/) and
[Bake](http://esrlabs.github.io/bake/) build systems. So far only Linux targets
have been tested; ports to other platforms are welcome.

### Using MCF in your own project

MCF is meant to be used as a (statically or dynamically linked) library. CMake
and Bake build files are provided. The library itself has three parts:
`mcf_core`, `mcf_remote`, and `mcf_cuda`. The main functionality is provided by
`mcf_core`, inter-process features reside in `mcf_remote`, CUDA-specific
features (such as wrappers around CUDA memory objects etc) can be found in
`mcf_cuda`.

Below is a minimal setup on how to use MCF to process a single integer value
```cpp
#include "mcf_core/Mcf.h"
#include <iostream>

const std::string inputTopicName = "/number";
const std::string outputTopicName = "/bigger_number";

/*
Any kind of message data has to be derived from `mcf::Value`. In this case, we
wrap an integer data type into a Value.
*/
struct Integer : public mcf::Value
{
public:
    explicit Integer(int64_t _value) : value(_value) {}
    
    int64_t value;
    MSGPACK_DEFINE(value);
};

/*
The class below defines a Component. A `Component` is roughly defined as a set
of `Port`s which are used to communicate `Value`s and port handlers, which
process incoming messages.

In this example, the `Component` reads an integer value whenever it is written
to the value store on the input topic, increments it, and writes the new value
to another topic.
*/
class MathComponent : public mcf::Component
{
public:
    // A constructor must initialize the `Component` sub-object with the name
    // of this component; the ports need to be initialized with the component
    // owning them and their name
    MathComponent() 
    : mcf::Component("MathComponent")
    , _numberInputPort(*this, "in_number")
    , _numberOutputPort(*this, "out_number")
    {
        // Assign a message handler for the input port
        _numberInputPort.registerHandler([this] () { handleInput(); });
    }
    
    // A `Component` must have a configure() method in which its `Port`s can
    // be made known to the system and tied to message topics.
    void configure(mcf::IComponentConfig& config) override
    {
        config.registerPort(_numberInputPort, inputTopicName);
        config.registerPort(_numberOutputPort, outputTopicName);
    }
    
private:
    // The actual message handler
    void handleInput()
    {
        // get a std::shared_ptr<const Integer> to the current input
        std::shared_ptr<const Integer> input = _numberInputPort.getValue();
        std::cout << input->value;
        // write a value to the output port
        _numberOutputPort.setValue(Integer(input->value + 1));
    }

    mcf::ReceiverPort<Integer> _numberInputPort;
    mcf::SenderPort<Integer> _numberOutputPort;
};

int main(int argc, char *argv)
{
    // Initialize parts of the middleware
    // The `ValueStore` is the central message exchange object, similar to a
    // database
    mcf::ValueStore valueStore;
    // The `ComponentManager` takes care of the `Component` lifecycle
    mcf::ComponentManager manager(valueStore);
    // The Component itself needs to be created...
    auto component = std::make_shared<MathComponent>();
    // ...and made known to the ComponentManager which will take care of it
    // from now on
    manager.registerComponent(component);
    // `configure()` sets up the wiring between all known`Component` s
    manager.configure();
    // `startup()` starts the event loop of each registered `Component`
    manager.startup();
    
    // In this minimalistic example we have only a single component and
    // therefore we talk to it by directly sending and receiving via the 
    // ValueStore. In a more realistic example, there would be another
    // component sending and receiving such values via ports.

    // Write a value to the value store
    valueStore.setValue(inputTopicName, Integer(41));
    // wait until the component is ready
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "The answer is" << valueStore.getValue(outputTopicName)->value << std::endl;
    
    // `shutdown()` stops the event loops
    manager.shutdown();
    return EXIT_SUCCESS;
}
```

