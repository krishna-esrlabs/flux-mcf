cmake_minimum_required(VERSION 3.19)
project(
  cmake_demo_cuda
  DESCRIPTION "Demo project using mcf and cuda compiled using cmake"
  LANGUAGES CUDA CXX)

# Generate project value types
add_subdirectory(mcf_cuda_demo_value_types)

find_package(CLI11 REQUIRED CONFIG)
find_package(McfCore REQUIRED CONFIG)
find_package(McfRemote REQUIRED CONFIG)
find_package(McfCuda REQUIRED CONFIG)

add_executable(McfCudaDemo
        src/Main.cpp
        src/ColourInverterComponent.cpp
        src/ImageFilterCudaComponent.cpp
        src/BoxFilter.cu
)

target_compile_features(McfCudaDemo
    INTERFACE
        cxx_std_11)

target_include_directories(McfCudaDemo
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(McfCudaDemo
    PRIVATE
        McfCore::McfCore
        McfRemote::McfRemote
        McfCuda::McfCuda
        McfCudaDemoValueTypes
        CLI11::CLI11
)
