// server/main.cpp
// Entry point for the CollabPad Phase 3 WebSocket server.
//
// Usage:
//   collabpad_server [port]
//
// If port is omitted, defaults to 8080.
// Press Ctrl+C to stop the server.

#include "WebSocketServer.hpp"
#include "DocumentHub.hpp"

#include <iostream>
#include <memory>
#include <cstdlib>   // std::atoi
#include <csignal>   // signal, SIGINT

// Global flag so Ctrl+C can gracefully stop the server.
// Using a volatile sig_atomic_t is the correct POSIX way to handle signals.
static volatile std::sig_atomic_t g_stopRequested = 0;

static void signalHandler(int /*sig*/) {
    g_stopRequested = 1;
}

// We keep a global pointer to the server so the signal handler can stop it.
// This is the only global mutable state in the server, justified by the
// requirement that signal handlers cannot easily communicate any other way.
static collabpad::WebSocketServer* g_server = nullptr;

static void onSignal(int sig) {
    std::cout << "\n[Main] Signal " << sig << " received. Stopping server...\n";
    if (g_server) {
        g_server->stop();
    }
    g_stopRequested = 1;
}

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    if (argc >= 2) {
        int p = std::atoi(argv[1]);
        if (p > 0 && p <= 65535) {
            port = static_cast<uint16_t>(p);
        } else {
            std::cerr << "[Main] Invalid port: " << argv[1]
                      << ". Using default 8080.\n";
        }
    }

    std::cout << "========================================\n"
              << "  CollabPad WebSocket Server  (Phase 3)\n"
              << "========================================\n";

    // Set up Ctrl+C handler
    std::signal(SIGINT, onSignal);

    auto hub = std::make_shared<collabpad::DocumentHub>();
    collabpad::WebSocketServer server(port, hub);
    g_server = &server;

    server.run(); // blocks until stop() is called

    std::cout << "[Main] Server exited cleanly.\n";
    return 0;
}
