add_subdirectory(mcf_remote_value_types)
find_package(cppzmq REQUIRED CONFIG)
find_package(CLI11 REQUIRED CONFIG)

find_package(GTest REQUIRED)

## Build McfRemoteUnitTestBase
add_executable(McfRemoteUnitTestBase
    src/remote_status_tracker_test.cpp
    src/zmq_msgpack_test.cpp
    src/remote_service_test.cpp
    src/remote_control_test.cpp
)

target_include_directories(McfRemoteUnitTestBase
    PRIVATE 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(McfRemoteUnitTestBase
    PRIVATE
        McfCore
        McfRemoteShmem
        GTest::gtest
        GTest::gtest_main
)

# Make unit tests runnable by calling ctest
gtest_discover_tests(McfRemoteUnitTestBase)

### Build RcTestEngine
add_executable(RcTestEngine
    src/rc_test_main.cpp
)
set_target_properties(RcTestEngine PROPERTIES OUTPUT_NAME "rcTestEngine")

target_include_directories(RcTestEngine
    PRIVATE 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(RcTestEngine
    PRIVATE
        McfCore
        McfRemoteShmem
        McfRemoteValueTypes
        CLI11::CLI11
)

### Build SimpleTest
add_executable(SimpleTest
    src/simple.cpp
)
set_target_properties(SimpleTest PROPERTIES OUTPUT_NAME "simple")
set_target_properties(SimpleTest PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(SimpleTest
    PRIVATE 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(SimpleTest
    PRIVATE
        McfCore
        McfRemote
        cppzmq
        pthread
)

### Build GeneratorTest
add_executable(GeneratorTest
    src/generator.cpp
)
set_target_properties(GeneratorTest PROPERTIES OUTPUT_NAME "generator")
set_target_properties(GeneratorTest PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(GeneratorTest
    PRIVATE 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(GeneratorTest
    PRIVATE
        McfCore
        McfRemoteShmem
        McfRemoteValueTypes
        cppzmq
        CLI11::CLI11
)

### Build ResponderTest
find_package(Boost REQUIRED)
add_executable(ResponderTest
    src/responder.cpp
)
set_target_properties(ResponderTest PROPERTIES OUTPUT_NAME "responder")
set_target_properties(ResponderTest PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(ResponderTest
    PRIVATE 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(ResponderTest
    PRIVATE
        McfCore
        McfRemoteShmem
        McfRemoteValueTypes
        cppzmq
        CLI11::CLI11
        Boost::boost
)

### Build PerfReceiverTest
add_executable(PerfReceiverTest
    perf/receiver.cpp
)
set_target_properties(PerfReceiverTest PROPERTIES OUTPUT_NAME "receiver")
set_target_properties(PerfReceiverTest PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(PerfReceiverTest
    PRIVATE 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>

        $<INSTALL_INTERFACE:perf>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/perf>
)

target_link_libraries(PerfReceiverTest
    PRIVATE
        McfCore
        McfRemote
        McfRemoteValueTypes
        pthread
        cppzmq
)

### Build PerfSenderTest
add_executable(PerfSenderTest
    perf/sender.cpp
)
set_target_properties(PerfSenderTest PROPERTIES OUTPUT_NAME "sender")
set_target_properties(PerfSenderTest PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(PerfSenderTest
    PRIVATE 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>

        $<INSTALL_INTERFACE:perf>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/perf>
)

target_link_libraries(PerfSenderTest
    PRIVATE
        McfCore
        McfRemote
        McfRemoteValueTypes
        pthread
        cppzmq
)
