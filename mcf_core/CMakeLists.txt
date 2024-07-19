### Build McfLoggerTracer
add_library(McfLoggerTracer
    SHARED 
        src/ComponentLoggerGlobals.cpp
        src/ComponentTraceGlobals.cpp
)
set_target_properties(McfLoggerTracer PROPERTIES OUTPUT_NAME "mcf_logger_tracer")
target_link_options(McfLoggerTracer PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libexports.txt")
set_target_properties(McfLoggerTracer PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(McfLoggerTracer
    PUBLIC 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

### Build McfCore
find_package(msgpack REQUIRED CONFIG)
find_package(spdlog REQUIRED CONFIG)
find_package(jsoncpp REQUIRED CONFIG)

file(GLOB MCF_CORE_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/util/*.cpp")
list(REMOVE_ITEM MCF_CORE_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ComponentLoggerGlobals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ComponentTraceGlobals.cpp
)

add_library(McfCore
    STATIC
        ${MCF_CORE_SOURCES}
)
add_library(McfCore::McfCore ALIAS McfCore)
set_target_properties(McfCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(McfCore
    PUBLIC 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(McfCore
    PUBLIC
        msgpackc-cxx
        spdlog::spdlog_header_only
        jsoncpp_lib

    PRIVATE
        dl
        pthread
        McfLoggerTracer
)

### Build tests
if (BUILD_TESTS)
    add_subdirectory(test)
endif()

############################################## Install McfCore
include(InstallTarget)
install_targets(EXPORT_TARGET_NAME McfCore ADDITIONAL_TARGET_NAMES McfLoggerTracer)
