cmake_minimum_required(VERSION 3.19)
project(
  mcf_cpu_demo
  DESCRIPTION "Demo project using mcf compiled using cmake"
  LANGUAGES CXX)

# Generate project value types
add_subdirectory(mcf_cpu_demo_value_types)

find_package(CLI11 REQUIRED CONFIG)
find_package(McfCore REQUIRED CONFIG)
find_package(McfRemote REQUIRED CONFIG)

add_executable(McfCpuDemo
        src/Main.cpp
        src/ColourInverterComponent.cpp
        src/ImageFilterComponent.cpp
)

target_include_directories(McfCpuDemo
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(McfCpuDemo
    PRIVATE
        McfCore::McfCore
        McfRemote::McfRemote
        McfCpuDemoValueTypes
        CLI11::CLI11
)

