### Build McfRemoteValueTypes
find_package(PythonInterp 3 REQUIRED)

add_library(McfRemoteValueTypes 
    INTERFACE
)

target_include_directories(McfRemoteValueTypes
INTERFACE 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Add target to generate 
set(PYTHON_VALUE_TYPE_GENERATOR ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../mcf_tools/types_generator/value_type_generator.py
    -i ${CMAKE_CURRENT_SOURCE_DIR}/value_types_json
    -o_cpp ${CMAKE_CURRENT_SOURCE_DIR}/include
    -o_py ${CMAKE_CURRENT_SOURCE_DIR}/python)
add_custom_target(
    McfRemoteValueTypesTypeGeneration
    COMMAND ${PYTHON_VALUE_TYPE_GENERATOR}
    COMMENT "Generating mcf_remote value types."
)
add_dependencies(McfRemoteValueTypes McfRemoteValueTypesTypeGeneration)

set(PYTHON_VALUE_TYPE_CLEANER ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../mcf_tools/types_generator/clean_generated_value_types.py
    -o_cpp ${CMAKE_CURRENT_SOURCE_DIR}/include
    -o_py ${CMAKE_CURRENT_SOURCE_DIR}/python
    -i ${CMAKE_CURRENT_SOURCE_DIR}/value_types_json)
add_custom_target(
    McfRemoteValueTypeCleaner
    COMMAND ${PYTHON_VALUE_TYPE_CLEANER}
    COMMENT "Cleaning mcf_remote value types."
)
add_dependencies(McfCleaner McfRemoteValueTypeCleaner)