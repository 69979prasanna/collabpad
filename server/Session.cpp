// server/Session.cpp
//
// WebSocket frame format (RFC 6455 §5):
//
//  Byte 0:  FIN(1) | RSV(3) | Opcode(4)
//  Byte 1:  MASK(1) | Payload len(7)
//           If payload len == 126 → next 2 bytes = actual length (big-endian)
//           If payload len == 127 → next 8 bytes = actual length (big-endian)
//  If MASK set: 4 masking-key bytes
//  Then: payload data (XOR-masked if MASK set)
//
// Opcodes we care about:
//   0x1 = text frame
//   0x8 = close
//   0x9 = ping → reply with pong (0xA)
//
// Client → Server messages are always masked (per RFC 6455 §5.3).
// Server → Client messages must NOT be masked (per RFC 6455 §5.1).

#include "Session.hpp"
#include "DocumentHub.hpp"

// Winsock2 must be included before windows.h
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

namespace collabpad {

// --------------------------------------------------------------------------
// Minimal JSON value extraction helpers (no dependency on a JSON library).
// We only need to read a handful of string fields from flat JSON objects.
// --------------------------------------------------------------------------

// Returns the value of the first occurrence of "key":"value" in json.
// Returns "" if not found.
static std::string jsonGetString(const std::string& json, const std::string& key) {
    // Look for "key":"
    std::string needle = "\"" + key + "\"";
    size_t kpos = json.find(needle);
    if (kpos == std::string::npos) return "";

    size_t colon = json.find(':', kpos + needle.size());
    if (colon == std::string::npos) return "";

    // Skip whitespace after colon
    size_t vstart = colon + 1;
    while (vstart < json.size() && (json[vstart] == ' ' || json[vstart] == '\t'
                                     || json[vstart] == '\r' || json[vstart] == '\n')) {
        ++vstart;
    }
    if (vstart >= json.size() || json[vstart] != '"') return "";
    ++vstart; // skip opening quote

    // Find closing quote (handle simple \")
    std::string val;
    for (size_t i = vstart; i < json.size(); ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            ++i; // skip escaped char
            val.push_back(json[i]);
        } else if (json[i] == '"') {
            break;
        } else {
            val.push_back(json[i]);
        }
    }
    return val;
}

// --------------------------------------------------------------------------
// Session
// --------------------------------------------------------------------------

Session::Session(uintptr_t sock, std::shared_ptr<DocumentHub> hub, uint32_t sessionId)
    : sock_(sock), hub_(hub), sessionId_(sessionId) {}

Session::~Session() {
    close();
}

void Session::close() {
    if (!alive_.exchange(false)) return; // already closed

    if (!docId_.empty()) {
        hub_->leave(docId_, sessionId_);
    }

    // Send a WebSocket close frame (opcode 0x8, no payload)
    uint8_t closeFrame[2] = { 0x88, 0x00 };
    sendRaw(reinterpret_cast<const char*>(closeFrame), 2);

    ::closesocket(static_cast<SOCKET>(sock_));
    std::cout << "[Session " << sessionId_ << "] Closed.\n";
}

bool Session::sendRaw(const char* data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        int n = ::send(static_cast<SOCKET>(sock_), data + sent,
                       static_cast<int>(length - sent), 0);
        if (n <= 0) {
            alive_ = false;
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

void Session::send(const std::string& message) {
    if (!alive_) return;

    // Build a WebSocket text frame (unmasked, server → client).
    // Opcode 0x1 = text.  FIN bit set.
    std::vector<uint8_t> frame;
    frame.push_back(0x81); // FIN + text opcode

    size_t len = message.size();
    if (len <= 125) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len <= 0xFFFF) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        }
    }
    // Append payload
    for (char c : message) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    sendRaw(reinterpret_cast<const char*>(frame.data()), frame.size());
}

// Read exactly n bytes from the socket into buf.  Returns false on error/close.
static bool recvExact(SOCKET sock, uint8_t* buf, size_t n) {
    size_t received = 0;
    while (received < n) {
        int r = ::recv(sock, reinterpret_cast<char*>(buf + received),
                       static_cast<int>(n - received), 0);
        if (r <= 0) return false;
        received += static_cast<size_t>(r);
    }
    return true;
}

std::string Session::readFrame() {
    if (!alive_) return "";

    SOCKET sock = static_cast<SOCKET>(sock_);

    // Read the first two header bytes
    uint8_t header[2];
    if (!recvExact(sock, header, 2)) {
        alive_ = false;
        return "";
    }

    bool fin    = (header[0] & 0x80) != 0;
    uint8_t opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t payloadLen = header[1] & 0x7F;

    // Extended payload length
    if (payloadLen == 126) {
        uint8_t ext[2];
        if (!recvExact(sock, ext, 2)) { alive_ = false; return ""; }
        payloadLen = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    } else if (payloadLen == 127) {
        uint8_t ext[8];
        if (!recvExact(sock, ext, 8)) { alive_ = false; return ""; }
        payloadLen = 0;
        for (int i = 0; i < 8; ++i) payloadLen = (payloadLen << 8) | ext[i];
    }

    // Read masking key (clients must mask; servers must not)
    uint8_t maskKey[4] = {0, 0, 0, 0};
    if (masked) {
        if (!recvExact(sock, maskKey, 4)) { alive_ = false; return ""; }
    }

    // Safety limit: refuse frames larger than 1 MiB to prevent memory exhaustion
    if (payloadLen > 1024 * 1024) {
        std::cerr << "[Session " << sessionId_ << "] Frame too large ("
                  << payloadLen << " bytes), closing.\n";
        alive_ = false;
        return "";
    }

    // Read payload
    std::vector<uint8_t> payload(static_cast<size_t>(payloadLen));
    if (payloadLen > 0) {
        if (!recvExact(sock, payload.data(), static_cast<size_t>(payloadLen))) {
            alive_ = false;
            return "";
        }
        if (masked) {
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] ^= maskKey[i % 4];
            }
        }
    }

    // Handle control frames
    if (opcode == 0x8) {
        // Close frame
        alive_ = false;
        return "";
    }
    if (opcode == 0x9) {
        // Ping → send pong
        std::vector<uint8_t> pong;
        pong.push_back(0x8A); // FIN + pong opcode
        pong.push_back(static_cast<uint8_t>(payloadLen & 0x7F));
        pong.insert(pong.end(), payload.begin(), payload.end());
        sendRaw(reinterpret_cast<const char*>(pong.data()), pong.size());
        return ""; // No application data in a ping
    }
    if (opcode != 0x1) {
        // Ignore binary (0x2) and continuation (0x0) frames for now
        return "";
    }

    // Text frame: return payload as string
    return std::string(payload.begin(), payload.end());
}

void Session::handleMessage(const std::string& json) {
    if (json.empty()) return;

    std::string type = jsonGetString(json, "type");
    if (type.empty()) {
        std::cerr << "[Session " << sessionId_ << "] Missing \"type\" field, ignoring.\n";
        return;
    }

    if (type == "join") {
        handleJoin(json);
    } else if (type == "op") {
        handleOperation(json);
    } else {
        std::cerr << "[Session " << sessionId_ << "] Unknown message type \""
                  << type << "\", ignoring.\n";
    }
}

void Session::handleJoin(const std::string& fullJson) {
    std::string newDocId = jsonGetString(fullJson, "docId");
    if (newDocId.empty()) {
        std::cerr << "[Session " << sessionId_ << "] join: missing or empty docId.\n";
        return;
    }
    // Basic validation: docId must be alphanumeric + dash + underscore, max 64 chars
    if (newDocId.size() > 64) {
        std::cerr << "[Session " << sessionId_ << "] join: docId too long.\n";
        return;
    }
    for (char c : newDocId) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') {
            std::cerr << "[Session " << sessionId_ << "] join: docId contains invalid character.\n";
            return;
        }
    }

    // Leave previous doc if already joined
    if (!docId_.empty() && docId_ != newDocId) {
        hub_->leave(docId_, sessionId_);
    }
    docId_ = newDocId;
    hub_->join(docId_, shared_from_this());

    // Send acknowledgement back to the joining client
    std::string ack = "{\"type\":\"joined\",\"docId\":\"" + docId_ + "\",\"sessionId\":"
                    + std::to_string(sessionId_) + "}";
    send(ack);
    std::cout << "[Session " << sessionId_ << "] Joined doc \"" << docId_ << "\"\n";
}

void Session::handleOperation(const std::string& fullJson) {
    if (docId_.empty()) {
        std::cerr << "[Session " << sessionId_ << "] op received before join, ignoring.\n";
        return;
    }
    std::string opDocId = jsonGetString(fullJson, "docId");
    if (opDocId != docId_) {
        std::cerr << "[Session " << sessionId_ << "] op docId mismatch, ignoring.\n";
        return;
    }
    // Broadcast the raw JSON (unchanged) to all other sessions in the same doc.
    hub_->broadcast(docId_, sessionId_, fullJson);
}

} // namespace collabpad
