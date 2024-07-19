
find_package(cppzmq REQUIRED CONFIG)
find_package(Boost REQUIRED)

### Build McfRemote
file(GLOB MCF_REMOTE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER MCF_REMOTE_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/Shmem.*.cpp")

add_library(McfRemote
    STATIC
        ${MCF_REMOTE_SOURCES}
)
add_library(McfRemote::McfRemote ALIAS McfRemote)
set_target_properties(McfRemote PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(McfRemote
    PUBLIC 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(McfRemote
    PUBLIC
        McfCore::McfCore
        cppzmq
        Boost::boost
    PRIVATE
        pthread
)

### Build McfRemoteShmem
file(GLOB MCF_REMOTE_SHMEM_LIB_SOURCES "src/*.cpp")

add_library(McfRemoteShmem
    STATIC
        ${MCF_REMOTE_SHMEM_LIB_SOURCES}
)
add_library(McfRemoteShmem::McfRemoteShmem ALIAS McfRemoteShmem)
target_compile_definitions(McfRemoteShmem PRIVATE HAVE_SHMEM)
set_target_properties(McfRemoteShmem PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(McfRemoteShmem
    PUBLIC 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(McfRemoteShmem
    PUBLIC
        McfCore::McfCore
        cppzmq
        Boost::boost
    PRIVATE
        pthread
        rt
)

### Build tests
if (BUILD_TESTS)
    add_subdirectory(test)
endif()

############################################## Install
include(InstallTarget)
install_targets(EXPORT_TARGET_NAME McfRemote)
install_targets(EXPORT_TARGET_NAME McfRemoteShmem)