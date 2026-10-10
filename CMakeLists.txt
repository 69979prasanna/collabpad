cmake_minimum_required(VERSION 3.15)
project(CollabPad VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Core library for CollabPad (header-only interface library)
add_library(collabpad_core INTERFACE)
target_include_directories(collabpad_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

# Phase 1 terminal demo (unchanged)
add_executable(collabpad_demo core/tests/phase1_demo.cpp)
target_link_libraries(collabpad_demo PRIVATE collabpad_core)

# Phase 2 terminal demo (unchanged)
add_executable(collabpad_phase2_demo core/tests/phase2_demo.cpp)
target_link_libraries(collabpad_phase2_demo PRIVATE collabpad_core)

# -------------------------------------------------------------------------
# Phase 3: WebSocket server
#
# Dependencies:
#   ws2_32  — Windows Sockets 2 (part of MinGW, no separate install needed)
#
# The server implements the WebSocket RFC 6455 handshake and framing itself
# using only Winsock2, so no third-party WebSocket library is required.
# -------------------------------------------------------------------------
add_executable(collabpad_server
    server/main.cpp
    server/WebSocketServer.cpp
    server/Session.cpp
    server/DocumentHub.cpp
)
target_include_directories(collabpad_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)
# ws2_32 provides WSAStartup, socket, select, send, recv, etc.
target_link_libraries(collabpad_server PRIVATE ws2_32)

