### Build McfCuda
file(GLOB MCF_CUDA_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

enable_language(CUDA)

find_package(CUDAToolkit REQUIRED)
find_package(Thrust REQUIRED CONFIG)
thrust_create_target(Thrust)

add_library(McfCuda
    SHARED 
        ${MCF_CUDA_SOURCES}
)
add_library(McfCuda::McfCuda ALIAS McfCuda)
set_target_properties(McfCuda PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_options(McfCuda PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libexports.txt")
target_compile_definitions(McfCuda PUBLIC HAVE_CUDA)

target_include_directories(McfCuda
    PUBLIC 
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(McfCuda
    PUBLIC
        McfCore::McfCore
        Thrust
        CUDA::cuda_driver
        CUDA::cudart
        CUDA::nvToolsExt
)

############################################## Install
include(InstallTarget)
install_targets(EXPORT_TARGET_NAME McfCuda)