# MCF Cuda Demo Project
This project contains a simple example which showcases many of the C++ and Python features of MCF. It contains a single, sequential pipeline which reads an image from a file, performs various image processing procedures and then displays the image results at each step of the pipeline.

The pipeline consists of the following parts:

* **main.cpp**: The main file sets up the required MCF infrastructure including the MCF Remote Control component, which facilitates communication between Python and C++. It also instantiates and registers 2 custom components (ColourInverterComponent and ImageFilterComponent) and runs in an infinite loop until a user interrupt is received.
* **process_images.py**: This script establishes the python-side connection of the MCF Remote Control, receives parameters used in the image processing from the user and reads an example image from file. It then sends the image and image processing parameters on their relevant topics to the C++-side Remote Control. The C++-side Remote Control receives these values and writes them to the value store, so that they can be read by other components.
* **ColourInverterComponent.cpp**: This component receives the image sent from process_images.py on a QueuedReceiverPort. Every time a new image is received, it inverts the image intensities by taking (255 - pixel_intensity) for each colour channel of each pixel in the image. It then sends the inverted image on a SenderPort.
* **ImageFilterComponent.cpp**: This component receives the inverted image sent from the ColourInverterComponent on a QueuedReceiverPort. Every time a new image is received, it convolves each pixel in the image in parallel with a [box filter](https://en.wikipedia.org/wiki/Box_blur) kernel implemented with Cuda. It then sends the blurred image on a SenderPort.
* **process_images.py** This script then continuously checks if certain values on the value store have been updated via the Python-side Remote Control. Once it has received all these values, it visualises them using cv2.

## Dependencies
* Requires mcf_core, mcf_cuda, and mcf_remote to be built and installed. Requires type generator python scripts from mcf_tools to be installed or on the PYTHONPATH.
* Requires python >= 3.8

## Building
Note. Ensure that MCF has been built and installed.
1) Get the install path of the cmake dependencies (optionally built by the install_deps.sh script in MCF) and the MCF installation (from `-DCMAKE_INSTALL_PREFIX` in the MCF installation instructions).
2) Build demo project:
        
        mkdir build
        cd build
        cmake .. -DCMAKE_PREFIX_PATH="/path/to/mcf/installation;/path/to/mcf/dependencies"
        make -j<number_of_cores>

## Running the demo
1) Setup python environment:
 
        pip install -r requirements.txt
2) The demo can be run by calling the run_demo.sh bash script, which optionally takes the kernel size for the box filter as input:
        
        bash run_demo.sh <kernel_size>