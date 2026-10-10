// server/WebSocketServer.cpp
//
// I/O model: select()-based single-threaded event loop.
//
// Why select() instead of threads?
//   - MinGW 6.3 has broken <thread> and <mutex> (no posix-thread model compiled in
//     by default in the mingw.org distribution — only the win32 thread model is
//     available, and std::mutex etc. are stub stubs that abort at runtime).
//   - select() is part of Winsock2 and works perfectly on MinGW 6.3.
//   - For the Phase 3 goal (routing messages between clients) a single-threaded
//     event loop is sufficient and avoids all locking complexity.
//
// Loop logic:
//   1. Add the listening socket to the readable set.
//   2. Add every live session socket to the readable set.
//   3. Call select() with a short timeout (100 ms) so the loop can check running_.
//   4. If the listening socket is readable, accept a new connection and upgrade it.
//   5. For each session socket that is readable, read one frame and process it.
//   6. Prune dead sessions.

#include "WebSocketServer.hpp"
#include "Session.hpp"
#include "DocumentHub.hpp"
#include "Sha1.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cctype>

// Link against ws2_32.lib — added in CMakeLists.txt

namespace collabpad {

// -------------------------------------------------------------------------
// HTTP upgrade helpers
// -------------------------------------------------------------------------

// Read the HTTP upgrade request from the socket.
// Returns the raw header text, or "" on error.
static std::string readHttpRequest(SOCKET sock) {
    std::string buf;
    buf.reserve(512);
    char c;
    while (buf.size() < 8192) { // safety limit
        int r = ::recv(sock, &c, 1, 0);
        if (r <= 0) return "";
        buf.push_back(c);
        // HTTP headers end with \r\n\r\n
        if (buf.size() >= 4 &&
            buf[buf.size()-4] == '\r' && buf[buf.size()-3] == '\n' &&
            buf[buf.size()-2] == '\r' && buf[buf.size()-1] == '\n') {
            break;
        }
    }
    return buf;
}

// Case-insensitive header value extraction.
// Returns the value of the first header line matching "HeaderName: value".
static std::string getHeader(const std::string& request, const std::string& name) {
    // Build lower-case version of the name for comparison
    std::string nameLower = name;
    for (char& c : nameLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::istringstream ss(request);
    std::string line;
    while (std::getline(ss, line)) {
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // Find colon
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        // trim trailing space from key
        while (!key.empty() && key.back() == ' ') key.pop_back();
        if (key == nameLower) {
            std::string val = line.substr(colon + 1);
            // trim leading space
            size_t start = val.find_first_not_of(" \t");
            return (start == std::string::npos) ? "" : val.substr(start);
        }
    }
    return "";
}

// Compute the Sec-WebSocket-Accept value per RFC 6455 §4.2.2.
static std::string computeAcceptKey(const std::string& clientKey) {
    static const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = clientKey + magic;
    auto digest = Sha1::compute(combined);
    return base64Encode(digest.data(), digest.size());
}

// Send the 101 Switching Protocols response.
static bool sendHandshakeResponse(SOCKET sock, const std::string& acceptKey) {
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + acceptKey + "\r\n"
        "\r\n";
    size_t sent = 0;
    while (sent < response.size()) {
        int r = ::send(sock,
                       response.c_str() + sent,
                       static_cast<int>(response.size() - sent), 0);
        if (r <= 0) return false;
        sent += static_cast<size_t>(r);
    }
    return true;
}

// -------------------------------------------------------------------------
// WebSocketServer
// -------------------------------------------------------------------------

WebSocketServer::WebSocketServer(uint16_t port, std::shared_ptr<DocumentHub> hub)
    : port_(port), hub_(hub) {
    // Initialise Winsock (must be done once per process)
    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != 0) {
        std::cerr << "[Server] WSAStartup failed: " << err << "\n";
    }
}

WebSocketServer::~WebSocketServer() {
    stop();
    WSACleanup();
}

void WebSocketServer::stop() {
    running_ = false;
    if (listenSock_ != (uintptr_t)(~0)) {
        ::closesocket(static_cast<SOCKET>(listenSock_));
        listenSock_ = (uintptr_t)(~0);
    }
}

std::shared_ptr<Session> WebSocketServer::doHandshake(uintptr_t clientSockHandle) {
    SOCKET clientSock = static_cast<SOCKET>(clientSockHandle);

    std::string request = readHttpRequest(clientSock);
    if (request.empty()) {
        std::cerr << "[Server] Empty HTTP request from client.\n";
        ::closesocket(clientSock);
        return nullptr;
    }

    // Validate it is a WebSocket upgrade request
    std::string upgrade   = getHeader(request, "Upgrade");
    std::string connection = getHeader(request, "Connection");
    std::string wsKey     = getHeader(request, "Sec-WebSocket-Key");

    // Case-insensitive check for "websocket"
    std::string upgradeLower = upgrade;
    for (char& c : upgradeLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (upgradeLower != "websocket" || wsKey.empty()) {
        // Respond with 400 Bad Request for non-WebSocket connections
        std::string bad = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        ::send(clientSock, bad.c_str(), static_cast<int>(bad.size()), 0);
        ::closesocket(clientSock);
        std::cerr << "[Server] Rejected non-WebSocket connection.\n";
        return nullptr;
    }

    // Trim any trailing whitespace from the key
    while (!wsKey.empty() && (wsKey.back() == '\r' || wsKey.back() == '\n'
                               || wsKey.back() == ' ')) {
        wsKey.pop_back();
    }

    std::string acceptKey = computeAcceptKey(wsKey);
    if (!sendHandshakeResponse(clientSock, acceptKey)) {
        ::closesocket(clientSock);
        std::cerr << "[Server] Failed to send handshake response.\n";
        return nullptr;
    }

    // Assign a unique session ID (simple incrementing counter)
    static uint32_t nextId = 1;
    uint32_t sessionId = nextId++;

    std::cout << "[Server] WebSocket handshake complete. Session " << sessionId << " connected.\n";
    return std::make_shared<Session>(
        static_cast<uintptr_t>(clientSock), hub_, sessionId);
}

void WebSocketServer::pruneDeadSessions() {
    sessions_.erase(
        std::remove_if(sessions_.begin(), sessions_.end(),
            [](const std::shared_ptr<Session>& s) {
                return !s || !s->isAlive();
            }),
        sessions_.end()
    );
    hub_->prune();
}

void WebSocketServer::run() {
    // Create the listening socket
    SOCKET listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "[Server] socket() failed: " << WSAGetLastError() << "\n";
        return;
    }
    listenSock_ = static_cast<uintptr_t>(listenSock);

    // Allow re-use of the port immediately after restart
    int opt = 1;
    ::setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (::bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[Server] bind() failed: " << WSAGetLastError() << "\n";
        return;
    }
    if (::listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[Server] listen() failed: " << WSAGetLastError() << "\n";
        return;
    }

    running_ = true;
    std::cout << "[Server] Listening on port " << port_ << " ...\n";
    std::cout << "[Server] Press Ctrl+C to stop.\n";

    while (running_) {
        // Build the fd_set from the listening socket + all live session sockets
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSock, &readSet);
        SOCKET maxFd = listenSock;

        for (const auto& s : sessions_) {
            if (s && s->isAlive()) {
                SOCKET fd = static_cast<SOCKET>(s->id()); // note: we store the socket separately
                // We need the actual socket fd — retrieve it via a helper.
                // Because Session::sock_ is private, we add a public accessor below.
                // For now we use a reinterpret trick via the session's sock accessor.
                // (See Session.hpp — we added sockFd() accessor)
                SOCKET sessionFd = static_cast<SOCKET>(s->sockFd());
                FD_SET(sessionFd, &readSet);
                if (sessionFd > maxFd) maxFd = sessionFd;
            }
        }

        // 100 ms timeout so we can re-check running_ and prune dead sessions
        timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 100000; // 100 ms

        int ready = ::select(static_cast<int>(maxFd + 1), &readSet, nullptr, nullptr, &timeout);
        if (ready == SOCKET_ERROR) {
            int e = WSAGetLastError();
            if (e == WSAEINTR) continue; // interrupted, retry
            std::cerr << "[Server] select() error: " << e << "\n";
            break;
        }

        // New connection?
        if (FD_ISSET(listenSock, &readSet)) {
            SOCKET clientSock = ::accept(listenSock, nullptr, nullptr);
            if (clientSock != INVALID_SOCKET) {
                auto session = doHandshake(static_cast<uintptr_t>(clientSock));
                if (session) {
                    sessions_.push_back(session);
                }
            }
        }

        // Readable session sockets
        for (const auto& s : sessions_) {
            if (!s || !s->isAlive()) continue;
            SOCKET sessionFd = static_cast<SOCKET>(s->sockFd());
            if (FD_ISSET(sessionFd, &readSet)) {
                std::string frame = s->readFrame();
                if (!frame.empty()) {
                    s->handleMessage(frame);
                }
                // If readFrame() caused alive_ to go false, pruneDeadSessions will clean up
            }
        }

        pruneDeadSessions();
    }

    std::cout << "[Server] Stopped.\n";
}

} // namespace collabpad
