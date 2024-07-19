# MCF Value Types Demo Project
This project contains a simple example which demonstrates how to generate and use MCF Value Types. For more information 
on Value Types and Value Type Generation, see [README.md](../../mcf_tools/types_generator/README.md) and 
[GETTING_STARTED.md](../../mcf_tools/types_generator/GETTING_STARTED.md), respectively.

* **main.cpp**: The main file sets up the required MCF infrastructure including the MCF Remote Control component, which 
facilitates communication between Python and C++. It also instantiates and registers the custom PoseReceiverComponent 
and runs for a fixed time before exiting.
* **send_pose.py**: This script establishes the python-side connection of the MCF Remote Control, and sends a pose message
to the C++-side Remote Control. The C++-side Remote Control receives these values and writes them to the value store, so 
that they can be read by other components.

## Dependencies
* Requires mcf_core and mcf_remote to be built and installed. 
* Requires type generator python scripts from mcf_tools to be installed or on the PYTHONPATH.
* Requires python >= 3.8

## Building
1) Get the install path of the cmake dependencies (optionally built by the install_deps.sh script in MCF) and the MCF installation (from `-DCMAKE_INSTALL_PREFIX` in the MCF installation instructions).
2) Build demo project:
        
        mkdir build
        cd build
        cmake .. -DCMAKE_PREFIX_PATH="/path/to/mcf/installation;/path/to/mcf/dependencies"
        make -j<number_of_cores>

## Running the demo
1) The demo can be run by calling the run_demo.sh bash script.
        
        bash run_demo.sh