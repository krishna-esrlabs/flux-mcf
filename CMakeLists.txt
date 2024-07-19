cmake_minimum_required(VERSION 3.19)
project(
  mcf
  DESCRIPTION "Mcf"
  LANGUAGES CXX)

# Set global flags
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-O3 -g)

# Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

# Add the cmake directory to the cmake module path
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Setup build options
option(BUILD_CUDA "Flag to build mcf_cuda" false)
option(BUILD_REMOTE "Flag to build mcf_remote" false)
option(BUILD_TESTS "Flag to build mcf_remote" false)

## Clean
# Add target for cleaning MCF. Cleaning target can be called using `make McfCleaner` in the build 
# directory. Any target that should be part of the MCF cleaning process should add a dependency on 
# McfCleaner. See mcf_remote/test/mcf_remote_value_types/CMakeLists.txt for an example
add_custom_target(
    McfCleaner
    COMMENT "Calling custom MCF cleaning commands."
)

if (BUILD_TESTS)
    enable_testing()
endif()

# Add mcf_core, mcf_remote, mcf_cuda and mcf_tools subdirectories based on build options
add_subdirectory(mcf_core)
add_subdirectory(mcf_tools)

if (BUILD_CUDA)
    add_subdirectory(mcf_cuda)
endif()

if (BUILD_REMOTE)
    add_subdirectory(mcf_remote)
endif()
