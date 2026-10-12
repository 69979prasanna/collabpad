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
#include "FileRepository.hpp"

#include <iostream>
#include <memory>
#include <cstdlib>   // atoi
#include <csignal>   // signal, SIGINT

using namespace std;

// Global flag so Ctrl+C can gracefully stop the server.
static volatile sig_atomic_t g_stopRequested = 0;

static collabpad::WebSocketServer* g_server = nullptr;
static shared_ptr<collabpad::DocumentHub> g_hub = nullptr;

static void onSignal(int sig) {
    cout << "\n[Main] Signal " << sig << " received. Stopping server...\n";
    if (g_hub) {
        g_hub->flushAll();
    }
    if (g_server) {
        g_server->stop();
    }
    g_stopRequested = 1;
}

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    string dataDir = "data/documents";

    if (argc >= 2) {
        int p = atoi(argv[1]);
        if (p > 0 && p <= 65535) {
            port = static_cast<uint16_t>(p);
        } else {
            cerr << "[Main] Invalid port: " << argv[1]
                 << ". Using default 8080.\n";
        }
    }
    if (argc >= 3) {
        dataDir = argv[2];
    }

    cout << "========================================\n"
         << "  CollabPad Real-Time Server  (Phase 5)\n"
         << "========================================\n";

    // Set up Ctrl+C handler
    signal(SIGINT, onSignal);

    auto repository = make_shared<collabpad::FileRepository>(dataDir);
    auto hub = make_shared<collabpad::DocumentHub>(repository);
    g_hub = hub;

    collabpad::WebSocketServer server(port, hub);
    g_server = &server;

    server.run(); // blocks until stop() is called

    hub->flushAll();
    cout << "[Main] Server exited cleanly.\n";
    return 0;
}
