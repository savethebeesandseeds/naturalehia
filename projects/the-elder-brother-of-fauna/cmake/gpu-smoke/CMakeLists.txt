cmake_minimum_required(VERSION 3.24)

project(NaturalehiaFaunaGpuSmoke LANGUAGES CXX)

set(
    NATURALEHIA_FAUNA_CUDA_ARCHITECTURES
    "86"
    CACHE STRING
    "CUDA architectures compiled by the standalone CUDA smoke target"
)
set(
    NATURALEHIA_FAUNA_TORCH_CUDA_ARCH_LIST
    "8.6"
    CACHE STRING
    "CUDA architectures exposed to LibTorch through TORCH_CUDA_ARCH_LIST"
)
if(NOT NATURALEHIA_FAUNA_CUDA_ARCHITECTURES)
    message(FATAL_ERROR "NATURALEHIA_FAUNA_CUDA_ARCHITECTURES must not be empty")
endif()
if(NOT NATURALEHIA_FAUNA_TORCH_CUDA_ARCH_LIST)
    message(FATAL_ERROR "NATURALEHIA_FAUNA_TORCH_CUDA_ARCH_LIST must not be empty")
endif()

set(CMAKE_CUDA_ARCHITECTURES "${NATURALEHIA_FAUNA_CUDA_ARCHITECTURES}")
enable_language(CUDA)
find_package(CUDAToolkit 13.1 REQUIRED)
set(ENV{TORCH_CUDA_ARCH_LIST} "${NATURALEHIA_FAUNA_TORCH_CUDA_ARCH_LIST}")
unset(CMAKE_CUDA_ARCHITECTURES)
unset(CMAKE_CUDA_ARCHITECTURES CACHE)
find_package(Torch 2.13.0 EXACT REQUIRED CONFIG)

enable_testing()

add_executable(cuda_smoke cuda_smoke.cu)
target_link_libraries(cuda_smoke PRIVATE CUDA::cudart)
set_target_properties(
    cuda_smoke
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CUDA_STANDARD 20
        CUDA_STANDARD_REQUIRED YES
        CUDA_ARCHITECTURES "${NATURALEHIA_FAUNA_CUDA_ARCHITECTURES}"
)
add_test(NAME cuda_smoke COMMAND cuda_smoke)

add_executable(libtorch_smoke libtorch_smoke.cpp)
target_link_libraries(libtorch_smoke PRIVATE "${TORCH_LIBRARIES}")
target_compile_features(libtorch_smoke PRIVATE cxx_std_20)
separate_arguments(TORCH_CXX_FLAGS_LIST NATIVE_COMMAND "${TORCH_CXX_FLAGS}")
target_compile_options(libtorch_smoke PRIVATE ${TORCH_CXX_FLAGS_LIST})
set_target_properties(libtorch_smoke PROPERTIES CXX_EXTENSIONS OFF)

get_filename_component(_libtorch_root "${Torch_DIR}/../../.." ABSOLUTE)
get_filename_component(_gpu_root "${_libtorch_root}/.." ABSOLUTE)
foreach(_component IN ITEMS cudnn nccl cusparselt nvshmem)
    set(_vendor_lib_dir "${_gpu_root}/nvidia/${_component}/lib")
    if(NOT IS_DIRECTORY "${_vendor_lib_dir}")
        message(FATAL_ERROR "LibTorch vendor runtime directory missing: ${_vendor_lib_dir}")
    endif()
    target_link_options(
        libtorch_smoke
        PRIVATE "LINKER:-rpath-link,${_vendor_lib_dir}"
    )
endforeach()
add_test(NAME libtorch_smoke COMMAND libtorch_smoke)
