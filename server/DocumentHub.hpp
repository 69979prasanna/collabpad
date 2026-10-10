// server/DocumentHub.hpp
// Keeps track of which sessions are subscribed to which document.
// When a session sends an operation, DocumentHub broadcasts it to every
// other session subscribed to the same document.
//
// Responsibilities:
//   - Register / unregister sessions by document ID.
//   - Broadcast a JSON string to all other sessions in the same document.
//   - Skip sessions that are no longer alive.
//
// Not responsible for any WebSocket or TCP details.
#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

namespace collabpad {

class Session;

class DocumentHub {
public:
    DocumentHub() = default;

    // Non-copyable
    DocumentHub(const DocumentHub&) = delete;
    DocumentHub& operator=(const DocumentHub&) = delete;

    // Register a session with a document. Called when a "join" message arrives.
    void join(const std::string& docId, std::shared_ptr<Session> session);

    // Remove a session from its document. Called on disconnect.
    void leave(const std::string& docId, uint32_t sessionId);

    // Broadcast a JSON message to every session in docId except the sender.
    void broadcast(const std::string& docId, uint32_t senderSessionId,
                   const std::string& message);

    // Remove dead sessions from all document lists (housekeeping).
    void prune();

private:
    // docId -> list of sessions subscribed to that document.
    std::unordered_map<std::string, std::vector<std::shared_ptr<Session>>> docs_;
};

} // namespace collabpad
