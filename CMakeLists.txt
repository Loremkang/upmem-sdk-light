cmake_minimum_required(VERSION 3.13)
project(upmem_sdk_light LANGUAGES CXX)
set(CMAKE_VERBOSE_MAKEFILE TRUE)
# set(CMAKE_CXX_COMPILER "g++")

set(UPMEM_SDK_DIR "$ENV{UPMEM_HOME}")
set(UPMEM_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/upmem-sdk/src)
if(NOT EXECUTABLE_OUTPUT_PATH)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
endif()

set(PARLAYLIB_INCLUDE_HEADER
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/parlaylib/include
        )

set(UPMEM_LIB_INCLUDE_HEADER
        ${UPMEM_SRC_DIR}
        ${UPMEM_SRC_DIR}/backends/api/include/api
        ${UPMEM_SRC_DIR}/backends/api/include/lowlevel
        ${UPMEM_SRC_DIR}/backends/api/src/include
        ${UPMEM_SRC_DIR}/backends/commons/include
        ${UPMEM_SRC_DIR}/backends/commons/src/properties
        ${UPMEM_SRC_DIR}/backends/hw/src/rank
        ${UPMEM_SRC_DIR}/backends/hw/src/commons
        ${UPMEM_SRC_DIR}/backends/ufi/include/ufi
        ${UPMEM_SRC_DIR}/backends/ufi/include
        ${UPMEM_SRC_DIR}/backends/verbose/src
        )

set(INCLUDE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pim_interface
        ${UPMEM_LIB_INCLUDE_HEADER}
        ${PARLAYLIB_INCLUDE_HEADER}
        )

set(EXAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/examples)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)


# cpu part for example
add_executable(example ${EXAMPLE_DIR}/host.cpp)
target_include_directories(example PUBLIC ${INCLUDE_DIR})
target_link_directories(example PUBLIC ${UPMEM_SDK_DIR}/lib)
target_link_libraries(example PUBLIC -ldpu)
target_link_libraries(example PUBLIC Threads::Threads)
target_compile_options(example PUBLIC -Wall -Wextra -O3 -g -std=c++17 -march=native)
set_target_properties(example PROPERTIES PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/upmem_direct_c.h)


# dpu part
set(DPU_PROGRAM_NAME dpu)
set(UPMEM_C_COMPILER ${UPMEM_SDK_DIR}/bin/dpu-upmem-dpurte-clang)

if(NOT NR_TASKLETS)
  set(NR_TASKLETS 12)
  message(WARNING "NR_TASKLETS is " ${NR_TASKLETS})
endif()

add_custom_target(${DPU_PROGRAM_NAME} ALL
    COMMAND ${UPMEM_C_COMPILER} -O3 -fgnu89-inline
            -DNR_TASKLETS=${NR_TASKLETS}
            -DSTACK_SIZE_DEFAULT=2048
            ${EXAMPLE_DIR}/dpu.c -o ${EXECUTABLE_OUTPUT_PATH}/${DPU_PROGRAM_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM)