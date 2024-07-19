cmake_minimum_required(VERSION 3.19)
project(
  mcf_value_type_demo
  DESCRIPTION "Example value type project"
  LANGUAGES CXX)

# Generate project value types
add_subdirectory(mcf_example_types)

find_package(McfCore REQUIRED CONFIG)
find_package(McfRemote REQUIRED CONFIG)

add_executable(McfValueTypeDemo
    src/Main.cpp
    src/PoseReceiverComponent.cpp
)
target_include_directories(McfValueTypeDemo
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(McfValueTypeDemo
    PRIVATE
        McfCore::McfCore
        McfRemote::McfRemote
        McfExampleTypes::McfExampleTypes
)