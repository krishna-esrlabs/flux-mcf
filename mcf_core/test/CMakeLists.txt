
### Build TestPluginDSO
add_library(TestPluginDso
    SHARED 
        src/plugin_dso.cpp
)
set_target_properties(TestPluginDso PROPERTIES OUTPUT_NAME "plugin_dso")
set_target_properties(TestPluginDso PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(TestPluginDso
    PUBLIC 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(TestPluginDso
    PRIVATE
        McfCore
)

### Build McfCoreUnitTestBase
find_package(GTest REQUIRED)

file(GLOB UNIT_TEST_BASE_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/util/*.cpp")
list(REMOVE_ITEM UNIT_TEST_BASE_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/test/src/plugin_dso.cpp
)

add_executable(McfCoreUnitTestBase
    ${UNIT_TEST_BASE_SOURCES}
)
target_compile_definitions(McfCoreUnitTestBase PRIVATE COMPILED_WITH_CMAKE)

target_include_directories(McfCoreUnitTestBase
    PUBLIC 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(McfCoreUnitTestBase
    PUBLIC
        McfCore::McfCore
    PRIVATE
        TestPluginDso
        GTest::gtest 
        GTest::gtest_main
        spdlog::spdlog_header_only
)

# Make unit tests runnable by calling ctest
gtest_discover_tests(McfCoreUnitTestBase)
