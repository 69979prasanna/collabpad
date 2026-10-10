// server/DocumentHub.cpp
#include "DocumentHub.hpp"
#include "Session.hpp"
#include <algorithm>
#include <iostream>

namespace collabpad {

void DocumentHub::join(const std::string& docId, std::shared_ptr<Session> session) {
    auto& list = docs_[docId];
    // Avoid double-registration for the same session id
    for (const auto& s : list) {
        if (s->id() == session->id()) {
            return;
        }
    }
    list.push_back(session);
    std::cout << "[Hub] Session " << session->id()
              << " joined document \"" << docId << "\". "
              << "Subscribers now: " << list.size() << "\n";
}

void DocumentHub::leave(const std::string& docId, uint32_t sessionId) {
    auto it = docs_.find(docId);
    if (it == docs_.end()) return;

    auto& list = it->second;
    list.erase(
        std::remove_if(list.begin(), list.end(),
            [sessionId](const std::shared_ptr<Session>& s) {
                return s->id() == sessionId;
            }),
        list.end()
    );
    std::cout << "[Hub] Session " << sessionId
              << " left document \"" << docId << "\". "
              << "Subscribers remaining: " << list.size() << "\n";
}

void DocumentHub::broadcast(const std::string& docId,
                             uint32_t senderSessionId,
                             const std::string& message) {
    auto it = docs_.find(docId);
    if (it == docs_.end()) return;

    for (const auto& s : it->second) {
        if (!s) continue;
        if (s->id() == senderSessionId) continue; // don't echo back to sender
        if (!s->isAlive()) continue;               // skip dead sessions
        s->send(message);
    }
}

void DocumentHub::prune() {
    for (auto& pair : docs_) {
        auto& list = pair.second;
        list.erase(
            std::remove_if(list.begin(), list.end(),
                [](const std::shared_ptr<Session>& s) {
                    return !s || !s->isAlive();
                }),
            list.end()
        );
    }
}

} // namespace collabpad
