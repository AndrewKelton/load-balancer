# Minimum CMake version required to configure this project
cmake_minimum_required(VERSION 3.20)

project(LoadBalancer LANGUAGES CXX)

# Require C++23 and use the standard flag (-std=c++23) instead of GNU extensions
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimized Release build if no build type was chosen
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Generate compile_commands.json so editors/clangd can find include paths
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizers
# Usage: cmake -B build -DSANITIZER=address (or thread, undefined, address,undefined)
set(SANITIZER "" CACHE STRING "Sanitizer to enable: address, thread, undefined, address,undefined")
set_property(CACHE SANITIZER PROPERTY STRINGS "" address thread undefined "address,undefined")

if(SANITIZER)
    if (SANITIZER MATCHES "thread" AND SANITIZER MATCHES "address")
        message(FATAL_ERROR "ThreadSanitizer and AddressSanitizer cannot be combined")
    endif()
    message(STATUS "Building with -fsanitize=${SANITIZER}")
    add_compile_options(-fsanitize=${SANITIZER} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${SANITIZER})
endif()

add_library(lb_core
    src/server.cpp
    src/utils.cpp
    src/load_balancer.cpp
    src/rate_limiter.cpp
    src/strategies/round_robin.cpp
    src/strategies/least_connections.cpp
)
target_include_directories(lb_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_options(lb_core PRIVATE -Wall -Wextra)

# Executable targets
add_executable(load_balancer src/main.cpp)
target_link_libraries(load_balancer PRIVATE lb_core)
target_compile_options(load_balancer PRIVATE -Wall -Wextra)

add_executable(backend src/backend_server.cpp)
target_link_libraries(backend PRIVATE lb_core)
target_compile_options(backend PRIVATE -Wall -Wextra)
# 
# # Header search paths
# target_include_directories(load_balancer PRIVATE
#     ${PROJECT_SOURCE_DIR}/include
# )
# target_include_directories(backend PRIVATE
#     ${PROJECT_SOURCE_DIR}/include
# )

# Compiler Warnings

# Enable testing infrastructure
enable_testing() 
add_subdirectory(tests) 