cmake_minimum_required(VERSION 3.19)
project(
  mcf_example_types
  DESCRIPTION "Example value type package"
  LANGUAGES CXX)

find_package(PythonInterp 3 REQUIRED)

add_library(McfExampleTypes
    INTERFACE
)
add_library(McfExampleTypes::McfExampleTypes ALIAS McfExampleTypes)
target_include_directories(McfExampleTypes
    INTERFACE
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

set(VALUE_TYPE_GENERATOR ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../mcf_tools/types_generator/value_type_generator.py
    -i ${CMAKE_CURRENT_SOURCE_DIR}/value_types_json
    -o_cpp ${CMAKE_CURRENT_SOURCE_DIR}/include
    -o_py ${CMAKE_CURRENT_SOURCE_DIR}/python)
add_custom_target(
    McfExampleTypesTypeGeneration
    COMMAND ${VALUE_TYPE_GENERATOR}
    COMMENT "Generating example value types."
)
add_dependencies(McfExampleTypes McfExampleTypesTypeGeneration)

set(VALUE_TYPE_CLEANER ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../mcf_tools/types_generator/clean_generated_value_types.py
    -o_cpp ${CMAKE_CURRENT_SOURCE_DIR}/include
    -o_py ${CMAKE_CURRENT_SOURCE_DIR}/python
    -i ${CMAKE_CURRENT_SOURCE_DIR}/value_types_json)
add_custom_target(
    McfExampleTypesTypeCleaner
    COMMAND ${VALUE_TYPE_CLEANER}
    COMMENT "Cleaning example value types."
)
