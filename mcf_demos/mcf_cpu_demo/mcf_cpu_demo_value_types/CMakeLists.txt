### Build McfCpuDemoValueTypes
find_package(PythonInterp 3 REQUIRED)

add_library(McfCpuDemoValueTypes
    INTERFACE
)
target_include_directories(McfCpuDemoValueTypes
    INTERFACE
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

set(PYTHON_VALUE_TYPE_GENERATOR ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../mcf_tools/types_generator/value_type_generator.py
    -i ${CMAKE_CURRENT_SOURCE_DIR}/value_types_json
    -o_cpp ${CMAKE_CURRENT_SOURCE_DIR}/include
    -o_py ${CMAKE_CURRENT_SOURCE_DIR}/python)
add_custom_target(
    ValueTypeGeneration
    COMMAND ${PYTHON_VALUE_TYPE_GENERATOR}
    COMMENT "Generating mcf_demo value types."
)
add_dependencies(McfCpuDemoValueTypes ValueTypeGeneration)

set(PYTHON_VALUE_TYPE_CLEANER ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../mcf_tools/types_generator/clean_generated_value_types.py
    -o_cpp ${CMAKE_CURRENT_SOURCE_DIR}/include
    -o_py ${CMAKE_CURRENT_SOURCE_DIR}/python
    -i ${CMAKE_CURRENT_SOURCE_DIR}/value_types_json)
add_custom_target(
    ValueTypeCleaner
    COMMAND ${PYTHON_VALUE_TYPE_CLEANER}
    COMMENT "Cleaning mcf_demo value types."
)
