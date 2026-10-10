// server/Session.hpp
// Represents one connected WebSocket client.
// Owns the client socket, reads frames, parses JSON messages,
// and interacts with DocumentHub for routing.
#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <atomic>

namespace collabpad {

class DocumentHub;

class Session : public std::enable_shared_from_this<Session> {
public:
    // sock:    the accepted, upgraded WebSocket socket descriptor.
    // hub:     shared DocumentHub for registering and broadcasting.
    // sessionId: unique numeric id for this session (for logging).
    Session(uintptr_t sock, std::shared_ptr<DocumentHub> hub, uint32_t sessionId);
    ~Session();

    // Non-copyable
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Read one WebSocket frame.  Returns the text payload, or "" on error/close.
    // Sets alive_ = false on connection close or error.
    std::string readFrame();

    // Send a text message to this client as a WebSocket text frame.
    // Thread-safety: must be called only from the server's single I/O thread.
    void send(const std::string& message);

    // Process a single incoming text message: parse JSON and dispatch.
    void handleMessage(const std::string& json);

    // Returns false once the underlying socket has been closed.
    bool isAlive() const { return alive_; }

    // The document this session joined, empty if not yet joined.
    const std::string& docId() const { return docId_; }

    uint32_t id() const { return sessionId_; }

    // The raw socket file descriptor — needed by WebSocketServer for select().
    uintptr_t sockFd() const { return sock_; }

    // Called by DocumentHub or the server to cleanly shut down the session.
    void close();

private:
    // Parse the "join" message type and register with DocumentHub.
    void handleJoin(const std::string& fullJson);

    // Parse and forward an "op" message to DocumentHub.
    void handleOperation(const std::string& fullJson);

    // Low-level: send raw bytes over the socket.
    bool sendRaw(const char* data, size_t length);

    uintptr_t sock_;
    std::shared_ptr<DocumentHub> hub_;
    uint32_t sessionId_;
    std::string docId_;       // empty until "join" is received
    std::atomic<bool> alive_{true};
};

} // namespace collabpad
