### Build ExampleValueTypes
find_package(PythonInterp 3 REQUIRED)

add_library(ExampleValueTypes
    INTERFACE
)
add_library(ExampleValueTypes::ExampleValueTypes ALIAS ExampleValueTypes)
target_include_directories(ExampleValueTypes
    INTERFACE
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        McfCore::McfCore
        ExampleValueTypesSecondary::ExampleValueTypesSecondary
)

set(VALUE_TYPE_GENERATOR ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../value_type_generator.py
    -i ${CMAKE_CURRENT_SOURCE_DIR}/value_types_json
    -o_cpp ${CMAKE_CURRENT_SOURCE_DIR}/include
    -o_py ${CMAKE_CURRENT_SOURCE_DIR}/python
    -i_link ${CMAKE_CURRENT_SOURCE_DIR}/../example_value_types_secondary/value_types_json)
add_custom_target(
    ExampleValueTypesTypeGeneration
    COMMAND ${VALUE_TYPE_GENERATOR}
    COMMENT "Generating example value types."
)
add_dependencies(ExampleValueTypes ExampleValueTypesTypeGeneration)

set(VALUE_TYPE_CLEANER ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../clean_generated_value_types.py
    -o_cpp ${CMAKE_CURRENT_SOURCE_DIR}/include
    -o_py ${CMAKE_CURRENT_SOURCE_DIR}/python
    -i ${CMAKE_CURRENT_SOURCE_DIR}/value_types_json)
add_custom_target(
    ExampleValueTypesTypeCleaner
    COMMAND ${VALUE_TYPE_CLEANER}
    COMMENT "Cleaning example value types."
)
add_dependencies(McfCleaner ExampleValueTypesTypeCleaner)