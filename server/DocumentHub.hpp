// server/DocumentHub.hpp
// Keeps track of active document rooms, sessions, collaborative presence,
// and CRDT state persistence.
#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <collabpad/Replica.hpp>
#include <collabpad/DocumentSnapshot.hpp>
#include "DocumentRepository.hpp"

namespace collabpad {

class Session;

struct CollaboratorInfo {
    uint32_t sessionId{0};
    uint32_t siteId{0};
    std::string name;
    std::string color;
    std::string status{"online"};
};

struct DocumentRoom {
    std::string docId;
    std::string title;
    std::unique_ptr<Replica> replica;
    std::vector<std::shared_ptr<Session>> sessions;
    std::unordered_map<uint32_t, CollaboratorInfo> collaborators; // sessionId -> info
    bool isDirty{false};
};

class DocumentHub {
public:
    explicit DocumentHub(std::shared_ptr<DocumentRepository> repository = nullptr);

    // Non-copyable
    DocumentHub(const DocumentHub&) = delete;
    DocumentHub& operator=(const DocumentHub&) = delete;

    void setRepository(std::shared_ptr<DocumentRepository> repo) {
        repository_ = repo;
    }

    std::shared_ptr<DocumentRepository> getRepository() const {
        return repository_;
    }

    // Register a session with a document. Called when a "join" message arrives.
    // Returns the current DocumentSnapshot of the room.
    DocumentSnapshot join(const std::string& docId, std::shared_ptr<Session> session,
                          uint32_t siteId = 0, const std::string& name = "");

    // Remove a session from its document. Called on disconnect.
    void leave(const std::string& docId, uint32_t sessionId);

    // Broadcast a JSON message to every session in docId except the sender.
    // If message is an "op", applies it to the room's CRDT replica and persists to repo.
    void broadcast(const std::string& docId, uint32_t senderSessionId,
                   const std::string& message);

    // Update collaborator presence information in the room
    void updatePresence(const std::string& docId, uint32_t sessionId,
                        uint32_t siteId, const std::string& name,
                        const std::string& color, const std::string& status);

    // Get list of active collaborators in a room
    std::vector<CollaboratorInfo> getCollaborators(const std::string& docId);

    // Get current snapshot for a room
    DocumentSnapshot getSnapshot(const std::string& docId);

    // Manually trigger saving a room to repository
    bool saveDocument(const std::string& docId);

    // Save all dirty documents (e.g. during server shutdown)
    void flushAll();

    // Remove dead sessions from all document lists (housekeeping).
    void prune();

private:
    DocumentRoom& getOrCreateRoom(const std::string& docId);
    void applyOpJson(DocumentRoom& room, const std::string& json);

    std::shared_ptr<DocumentRepository> repository_;
    std::unordered_map<std::string, DocumentRoom> rooms_;
};

} // namespace collabpad
