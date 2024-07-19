### Build ExampleValueTypesTest
find_package(PythonInterp 3 REQUIRED)

add_executable(ExampleValueTypesTest
    ${CMAKE_CURRENT_SOURCE_DIR}/main_test/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ComplexTypesTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SimpleTypesTest.cpp
)

target_include_directories(ExampleValueTypesTest
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(ExampleValueTypesTest
    PUBLIC
        McfCore::McfCore
        ExampleValueTypes::ExampleValueTypes
    PRIVATE
        McfRemote::McfRemote
        ExampleValueTypesSecondary::ExampleValueTypesSecondary
)

set(TEST_GENERATOR ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../tester_generator.py
    -i ${CMAKE_CURRENT_SOURCE_DIR}/../example_value_types/value_types_json
    -o ${CMAKE_CURRENT_SOURCE_DIR}
    -m ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../modules/mcf
    -x ${CMAKE_CURRENT_BINARY_DIR}/ExampleValueTypesTest)
add_custom_command(
    OUTPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/main_test/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ComplexTypesTest.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/SimpleTypesTest.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/example_value_types_test/ComplexTypesTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/example_value_types_test/SimpleTypesTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/python/types_test/ComplexTypesTest.py
        ${CMAKE_CURRENT_SOURCE_DIR}/python/types_test/SimpleTypesTest.py
        ${CMAKE_CURRENT_SOURCE_DIR}/python/test_example_value_types.py
    COMMAND ${TEST_GENERATOR}
    COMMENT "Generating example value type tests."
)

set(TEST_CLEANER ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../clean_generated_value_types.py
    -o_cpp ${CMAKE_CURRENT_SOURCE_DIR}/include
    -o_py ${CMAKE_CURRENT_SOURCE_DIR}/python
    -i ${CMAKE_CURRENT_SOURCE_DIR}/value_types_json)
add_custom_target(
    ExampleValueTypesTestCleaner
    COMMAND ${TEST_CLEANER}
    COMMENT "Cleaning example value type tests."
)
add_dependencies(McfCleaner ExampleValueTypesTestCleaner)
