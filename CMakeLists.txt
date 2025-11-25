cmake_minimum_required(VERSION 3.20)
project(ModuleInspector LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_CLIENT "Build the user-mode client" ON)
option(ENABLE_DRIVER "Build the kernel-mode driver (requires WDK on Windows)" OFF)
option(ENABLE_HOST_TESTS "Build portable layout tests" ON)

add_library(moduleinspector_common INTERFACE)
target_include_directories(moduleinspector_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(src)

if(ENABLE_HOST_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
