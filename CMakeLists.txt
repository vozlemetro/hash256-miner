cmake_minimum_required(VERSION 3.18)
project(hash256_cuda VERSION 1.0.0 LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES "61;70;75;80;86;89")
endif()

set(CMAKE_CUDA_FLAGS_RELEASE "-O3 --use_fast_math -DNDEBUG")
set(CMAKE_CUDA_FLAGS_DEBUG "-g -G -O0")

set(CUDA_SOURCES
    src/miner_kernel.cu
)
set(CPP_SOURCES
    src/main.cpp
    src/keccak256_host.cpp
    src/rpc_client.cpp
    src/tx_builder.cpp
    src/signer.cpp
)
set(HEADERS
    src/types.h
    src/keccak256.cuh
    src/miner_kernel.cuh
    src/rpc_client.h
    src/tx_builder.h
    src/signer.h
)

add_executable(hash256_cuda ${CUDA_SOURCES} ${CPP_SOURCES} ${HEADERS})
target_include_directories(hash256_cuda PRIVATE ${CMAKE_SOURCE_DIR}/src)

set_target_properties(hash256_cuda PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_RESOLVE_DEVICE_SYMBOLS ON
)

find_package(CUDAToolkit REQUIRED)
target_include_directories(hash256_cuda PRIVATE ${CUDAToolkit_INCLUDE_DIRS})

find_package(Threads REQUIRED)
target_link_libraries(hash256_cuda Threads::Threads)

find_package(CURL REQUIRED)
target_include_directories(hash256_cuda PRIVATE ${CURL_INCLUDE_DIRS})
target_link_libraries(hash256_cuda ${CURL_LIBRARIES})

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SECP256K1 libsecp256k1)
endif()
if(SECP256K1_FOUND)
    target_include_directories(hash256_cuda PRIVATE ${SECP256K1_INCLUDE_DIRS})
    target_link_libraries(hash256_cuda ${SECP256K1_LIBRARIES})
else()
    find_library(SECP256K1_LIB secp256k1)
    if(SECP256K1_LIB)
        target_link_libraries(hash256_cuda ${SECP256K1_LIB})
    else()
        message(FATAL_ERROR "libsecp256k1 not found! Install: sudo apt install libsecp256k1-dev")
    endif()
endif()

target_link_libraries(hash256_cuda ${CUDA_LIBRARIES})

target_compile_options(hash256_cuda PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic -Wno-unused-parameter>
    $<$<COMPILE_LANGUAGE:CUDA>:--ptxas-options=-v>
)

install(TARGETS hash256_cuda DESTINATION bin)

message(STATUS "Build: ${CMAKE_BUILD_TYPE}")
message(STATUS "CUDA arch: ${CMAKE_CUDA_ARCHITECTURES}")
