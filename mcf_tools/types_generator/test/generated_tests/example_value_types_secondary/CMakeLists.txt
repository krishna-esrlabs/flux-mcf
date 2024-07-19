### Build ExampleValueTypesSecondary
find_package(PythonInterp 3 REQUIRED)

add_library(ExampleValueTypesSecondary
    INTERFACE
)
add_library(ExampleValueTypesSecondary::ExampleValueTypesSecondary ALIAS ExampleValueTypesSecondary)
target_include_directories(ExampleValueTypesSecondary
    INTERFACE
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        McfCore::McfCore
)

set(VALUE_TYPE_GENERATOR ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../value_type_generator.py
    -i ${CMAKE_CURRENT_SOURCE_DIR}/value_types_json
    -o_cpp ${CMAKE_CURRENT_SOURCE_DIR}/include
    -o_py ${CMAKE_CURRENT_SOURCE_DIR}/python)
add_custom_target(
    ExampleValueTypesSecondaryTypeGeneration
    COMMAND ${VALUE_TYPE_GENERATOR}
    COMMENT "Generating secondary example value types."
)
add_dependencies(ExampleValueTypesSecondary ExampleValueTypesSecondaryTypeGeneration)

set(VALUE_TYPE_CLEANER ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../clean_generated_value_types.py
    -o_cpp ${CMAKE_CURRENT_SOURCE_DIR}/include
    -o_py ${CMAKE_CURRENT_SOURCE_DIR}/python
    -i ${CMAKE_CURRENT_SOURCE_DIR}/value_types_json)
add_custom_target(
    ExampleValueTypesSecondaryTypeCleaner
    COMMAND ${VALUE_TYPE_CLEANER}
    COMMENT "Cleaning example value types."
)
add_dependencies(McfCleaner ExampleValueTypesSecondaryTypeCleaner)