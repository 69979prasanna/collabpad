// server/DocumentHub.cpp
#include "DocumentHub.hpp"
#include "Session.hpp"
#include <algorithm>
#include <iostream>
#include <cctype>

using namespace std;

namespace collabpad {

// Helpers for parsing operations from JSON without third-party dependencies
static uint64_t parseSubInt(const string& json, size_t startPos, const string& key) {
    string needle = "\"" + key + "\"";
    size_t kpos = json.find(needle, startPos);
    if (kpos == string::npos) return 0;

    size_t colon = json.find(':', kpos + needle.size());
    if (colon == string::npos) return 0;

    size_t vstart = colon + 1;
    while (vstart < json.size() && (json[vstart] == ' ' || json[vstart] == '\t')) {
        ++vstart;
    }
    uint64_t val = 0;
    while (vstart < json.size() && isdigit(static_cast<unsigned char>(json[vstart]))) {
        val = val * 10 + (json[vstart] - '0');
        ++vstart;
    }
    return val;
}

static char parseCharValue(const string& json, size_t startPos) {
    string needle = "\"value\"";
    size_t kpos = json.find(needle, startPos);
    if (kpos == string::npos) return '\0';

    size_t colon = json.find(':', kpos + needle.size());
    if (colon == string::npos) return '\0';

    size_t vstart = colon + 1;
    while (vstart < json.size() && (json[vstart] == ' ' || json[vstart] == '\t')) {
        ++vstart;
    }
    if (vstart >= json.size() || json[vstart] != '"') return '\0';
    ++vstart;

    if (vstart < json.size()) {
        if (json[vstart] == '\\' && vstart + 1 < json.size()) {
            char next = json[vstart + 1];
            if (next == 'n') return '\n';
            if (next == 'r') return '\r';
            if (next == 't') return '\t';
            return next;
        }
        return json[vstart];
    }
    return '\0';
}

DocumentHub::DocumentHub(shared_ptr<DocumentRepository> repository)
    : repository_(repository) {}

DocumentRoom& DocumentHub::getOrCreateRoom(const string& docId) {
    auto it = rooms_.find(docId);
    if (it != rooms_.end()) {
        return it->second;
    }

    DocumentRoom room;
    room.docId = docId;
    room.title = docId;
    room.replica = make_unique<Replica>(0); // Server replica site 0
    room.isDirty = false;

    if (repository_) {
        if (repository_->exists(docId)) {
            DocumentSnapshot snap;
            if (repository_->load(docId, snap)) {
                room.replica->loadSnapshot(snap);
                room.title = snap.title.empty() ? docId : snap.title;
                cout << "[Hub] Restored document \"" << docId << "\" with "
                     << snap.characters.size() << " characters from storage.\n";
            }
        } else {
            // First time room is accessed: persist initial document snapshot
            DocumentSnapshot initSnap = room.replica->createSnapshot(docId, room.title);
            repository_->save(initSnap);
            cout << "[Hub] Created new persistent document room \"" << docId << "\".\n";
        }
    }

    auto inserted = rooms_.emplace(docId, move(room));
    return inserted.first->second;
}

DocumentSnapshot DocumentHub::join(const string& docId, shared_ptr<Session> session,
                                  uint32_t siteId, const string& name) {
    auto& room = getOrCreateRoom(docId);

    // Avoid double-registration for the same session id
    bool found = false;
    for (const auto& s : room.sessions) {
        if (s && s->id() == session->id()) {
            found = true;
            break;
        }
    }
    if (!found) {
        room.sessions.push_back(session);
    }

    if (siteId != 0) {
        CollaboratorInfo info;
        info.sessionId = session->id();
        info.siteId = siteId;
        info.name = name.empty() ? ("User " + to_string(siteId)) : name;
        info.color = "#6366f1";
        info.status = "online";
        room.collaborators[session->id()] = info;
    }

    cout << "[Hub] Session " << session->id()
         << " joined document \"" << docId << "\". "
         << "Subscribers now: " << room.sessions.size() << "\n";

    return room.replica->createSnapshot(room.docId, room.title);
}

void DocumentHub::leave(const string& docId, uint32_t sessionId) {
    auto it = rooms_.find(docId);
    if (it == rooms_.end()) return;

    auto& room = it->second;

    // Check if collaborator info exists and broadcast leave event to room
    auto cit = room.collaborators.find(sessionId);
    if (cit != room.collaborators.end()) {
        uint32_t siteId = cit->second.siteId;
        string name = cit->second.name;
        room.collaborators.erase(cit);

        string leaveMsg = "{\"type\":\"presence\",\"docId\":\"" + docId + "\","
                        + "\"siteId\":" + to_string(siteId) + ","
                        + "\"name\":\"" + name + "\","
                        + "\"status\":\"offline\",\"event\":\"leave\"}";
        broadcast(docId, sessionId, leaveMsg);
    }

    auto& list = room.sessions;
    list.erase(
        remove_if(list.begin(), list.end(),
            [sessionId](const shared_ptr<Session>& s) {
                return !s || s->id() == sessionId;
            }),
        list.end()
    );

    cout << "[Hub] Session " << sessionId
         << " left document \"" << docId << "\". "
         << "Subscribers remaining: " << list.size() << "\n";

    if (room.isDirty && repository_) {
        saveDocument(docId);
    }
}

void DocumentHub::applyOpJson(DocumentRoom& room, const string& json) {
    size_t opPos = json.find("\"op\"");
    if (opPos == string::npos) return;

    size_t idPos = json.find("\"id\"", opPos);
    size_t afterPos = json.find("\"afterId\"", opPos);
    if (idPos == string::npos || afterPos == string::npos) return;

    uint32_t idSite = static_cast<uint32_t>(parseSubInt(json, idPos, "siteId"));
    uint64_t idClock = parseSubInt(json, idPos, "clock");
    uint32_t afterSite = static_cast<uint32_t>(parseSubInt(json, afterPos, "siteId"));
    uint64_t afterClock = parseSubInt(json, afterPos, "clock");
    char val = parseCharValue(json, opPos);

    if (idClock > 0 && val != '\0') {
        InsertionOp op(CharId(idSite, idClock), CharId(afterSite, afterClock), val);
        room.replica->applyOperation(op);
        room.isDirty = true;

        // Persist update immediately to repository for durable recovery
        if (repository_) {
            DocumentSnapshot snap = room.replica->createSnapshot(room.docId, room.title);
            if (!repository_->save(snap)) {
                cerr << "[Hub] WARNING: Failed to persist edit to document \""
                     << room.docId << "\"\n";
            }
            room.isDirty = false;
        }
    }
}

void DocumentHub::broadcast(const string& docId,
                            uint32_t senderSessionId,
                            const string& message) {
    auto it = rooms_.find(docId);
    if (it == rooms_.end()) return;

    auto& room = it->second;

    // Check if this message is an operation to update server CRDT & persist
    if (message.find("\"type\":\"op\"") != string::npos ||
        message.find("\"type\": \"op\"") != string::npos) {
        applyOpJson(room, message);
    } else if (message.find("\"type\":\"presence\"") != string::npos ||
               message.find("\"type\": \"presence\"") != string::npos) {
        // Track collaborator info if present
        auto cit = room.collaborators.find(senderSessionId);
        if (cit != room.collaborators.end()) {
            if (message.find("\"editing\"") != string::npos) {
                cit->second.status = "editing";
            } else if (message.find("\"idle\"") != string::npos) {
                cit->second.status = "idle";
            }
        }
    }

    // Broadcast message to all other live sessions in the room
    for (const auto& s : room.sessions) {
        if (!s) continue;
        if (s->id() == senderSessionId) continue; // don't echo back
        if (!s->isAlive()) continue;              // skip dead sessions
        s->send(message);
    }
}

void DocumentHub::updatePresence(const string& docId, uint32_t sessionId,
                                uint32_t siteId, const string& name,
                                const string& color, const string& status) {
    auto it = rooms_.find(docId);
    if (it == rooms_.end()) return;

    CollaboratorInfo info;
    info.sessionId = sessionId;
    info.siteId = siteId;
    info.name = name;
    info.color = color;
    info.status = status;
    it->second.collaborators[sessionId] = info;
}

vector<CollaboratorInfo> DocumentHub::getCollaborators(const string& docId) {
    vector<CollaboratorInfo> result;
    auto it = rooms_.find(docId);
    if (it == rooms_.end()) return result;

    for (const auto& pair : it->second.collaborators) {
        result.push_back(pair.second);
    }
    return result;
}

DocumentSnapshot DocumentHub::getSnapshot(const string& docId) {
    auto& room = getOrCreateRoom(docId);
    return room.replica->createSnapshot(room.docId, room.title);
}

bool DocumentHub::saveDocument(const string& docId) {
    if (!repository_) return false;
    auto it = rooms_.find(docId);
    if (it == rooms_.end()) return false;

    DocumentSnapshot snap = it->second.replica->createSnapshot(docId, it->second.title);
    bool ok = repository_->save(snap);
    if (ok) {
        it->second.isDirty = false;
    }
    return ok;
}

void DocumentHub::flushAll() {
    if (!repository_) return;
    for (auto& pair : rooms_) {
        if (pair.second.isDirty) {
            saveDocument(pair.first);
        }
    }
}

void DocumentHub::prune() {
    for (auto& pair : rooms_) {
        auto& room = pair.second;
        auto& list = room.sessions;
        list.erase(
            remove_if(list.begin(), list.end(),
                [&room](const shared_ptr<Session>& s) {
                    if (!s || !s->isAlive()) {
                        if (s) {
                            room.collaborators.erase(s->id());
                        }
                        return true;
                    }
                    return false;
                }),
            list.end()
        );
    }
}

} // namespace collabpad
