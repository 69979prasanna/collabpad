cmake_minimum_required(VERSION 3.15)
project(CollabPad VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Core library for Phase 1
add_library(collabpad_core INTERFACE)
target_include_directories(collabpad_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

# Phase 1 terminal demo
add_executable(collabpad_demo core/tests/phase1_demo.cpp)
target_link_libraries(collabpad_demo PRIVATE collabpad_core)
