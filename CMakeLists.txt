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

add_library(lb_core
    src/server.cpp
    src/utils.cpp
    src/load_balancer.cpp
    src/rate_limiter.cpp
    src/strategies/round_robin.cpp
    src/strategies/least_connections.cpp
)
target_include_directories(lb_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

# Executable targets
add_executable(load_balancer src/main.cpp)
target_link_libraries(load_balancer PRIVATE lb_core)

add_executable(backend src/backend_server.cpp)
target_link_libraries(backend PRIVATE lb_core)

# Header search paths
target_include_directories(load_balancer PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(backend PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Compiler Warnings
target_compile_options(load_balancer PRIVATE -Wall -Wextra)
target_compile_options(backend PRIVATE -Wall -Wextra)

# Enable testing infrastructure
enable_testing() 
add_subdirectory(tests) 