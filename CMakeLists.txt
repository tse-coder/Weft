cmake_minimum_required(VERSION 3.14)
project(Weft VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler flags for optimization and warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -pthread)
endif()

# Weft core library
add_library(weft_core INTERFACE)
target_include_directories(weft_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Main executable (CLI Dashboard later)
add_executable(weft_cli src/main.cpp src/dashboard.cpp)
target_link_libraries(weft_cli PRIVATE weft_core)
