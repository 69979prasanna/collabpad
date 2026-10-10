// server/WebSocketServer.hpp
// Accepts TCP connections and upgrades each one to a WebSocket connection.
// Uses Winsock2 + select() for I/O multiplexing on Windows (MinGW compatible).
// Each accepted connection is handed off to Session for message handling.
#pragma once

#include <cstdint>
#include <memory>
#include <atomic>
#include <vector>
#include <functional>

// Forward declaration
namespace collabpad {
class Session;
class DocumentHub;

class WebSocketServer {
public:
    // port: TCP port to listen on (default 8080).
    // hub:  shared DocumentHub used by all sessions for routing.
    explicit WebSocketServer(uint16_t port, std::shared_ptr<DocumentHub> hub);
    ~WebSocketServer();

    // Non-copyable
    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;

    // Start the main I/O loop. Blocks until stop() is called.
    void run();

    // Signal the server to stop accepting new connections and exit run().
    void stop();

private:
    // Perform the WebSocket HTTP upgrade handshake on an accepted socket.
    // Returns the session if the upgrade succeeded, or nullptr on failure.
    std::shared_ptr<Session> doHandshake(uintptr_t clientSock);

    // Remove sessions that have disconnected.
    void pruneDeadSessions();

    uint16_t port_;
    std::shared_ptr<DocumentHub> hub_;
    std::atomic<bool> running_{false};

    uintptr_t listenSock_{(uintptr_t)(~0)}; // INVALID_SOCKET sentinel

    // Live sessions; pruned when they close.
    std::vector<std::shared_ptr<Session>> sessions_;
};

} // namespace collabpad
