# Technical Concerns & Future Considerations

## Technical Debt & Known Limitations

This document tracks known limitations, performance bottlenecks, technical debt, and architectural risks identified across the CollabPad codebase.

---

## 1. Algorithmic Gaps (CRDT)

### Missing Deletion Operations (Tombstones)
- **Status**: Critical functional limitation.
- **Current Behavior**: The core CRDT engine (`Sequence`, `Replica`, `DocumentSnapshot`, and `engineBridge.js`) only implements character insertions (`InsertionOp` / `insertAfter`). Text deletions (backspace, delete key, text highlighting + replacement) are not implemented.
- **Risk**: Users can insert characters concurrently, but deleting characters does not synchronize through the CRDT; text replacements in `Editor.jsx` only produce insert operations for novel characters.
- **Remediation**:
  - Introduce `DeletionOp` with target `CharId`.
  - Add `bool isDeleted{false}` (tombstone) flag to `Character` struct in `core/include/collabpad/Sequence.hpp`.
  - Keep tombstones in the sequence tree for causal positioning, filtering them out only during `getText()` visual rendering.
  - Implement periodic tombstone garbage collection / pruning protocol when all connected replicas confirm receipt.

### Single-Byte `char` vs. Multi-Byte UTF-8
- **Current Behavior**: `Character.value` is declared as a C++ `char` (1 byte).
- **Risk**: Multi-byte UTF-8 characters (e.g., accented characters, emojis, non-Latin scripts) require 2 to 4 bytes. Currently, each byte is stored as a distinct `Character` node with its own `CharId`. If two users concurrently type multi-byte characters at the same visual position, sibling interleaving could interleave raw bytes, corrupting the UTF-8 sequence.
- **Remediation**: Expand `Character.value` to `std::string` or `uint32_t` (Unicode code point).

---

## 2. Server Scalability & Performance Bottlenecks

### Winsock2 `select()` and `FD_SETSIZE` Socket Limit
- **Current Behavior**: The server uses a single-threaded event loop driven by `select()`. On Windows with MinGW, `winsock2.h` defaults `FD_SETSIZE` to 64 sockets.
- **Risk**: The server cannot support more than 63 active client sessions simultaneously. Exceeding this limit will cause socket descriptor overflow or dropped connections.
- **Remediation**:
  - Short-term: Define `#define FD_SETSIZE 1024` before `#include <winsock2.h>`.
  - Long-term: Transition to Windows I/O Completion Ports (IOCP) or `WSAEventSelect` for high-concurrency event-driven I/O.

### Synchronous Disk Persistence on Every Single Operation
- **Current Behavior**: In `DocumentHub::broadcast()`, whenever an incoming `"op"` message is received, `saveDocument(docId)` is called synchronously:
  - Generates the complete JSON snapshot of the entire document.
  - Writes to `<docId>.tmp`.
  - Replaces `<docId>.json` via Win32 `MoveFileExA`.
- **Risk**: During fast typing sessions with multiple collaborators (e.g. 5–10 users generating 50 operations/sec), the server performs 50 complete disk file rewrites and atomic renames per second on the single event loop thread. This can introduce noticeable frame processing latency.
- **Remediation**:
  - Implement an append-only operation log (WAL).
  - Debounce snapshot flushing: Mark `isDirty = true` on operation and persist to disk periodically (e.g. every 2 seconds or after 100 operations).

---

## 3. Client & Frontend Architecture

### Granular Character Diffing on Paste
- **Current Behavior**: `Editor.jsx` / `App.jsx` uses simple common-prefix/common-suffix string comparison. When large blocks of text are pasted (e.g. 500 lines of code), the client generates individual `localInsertAfter` calls in a tight loop and transmits hundreds of individual WebSocket packets.
- **Risk**: Floods the WebSocket connection and triggers dozens of immediate disk writes on the server.
- **Remediation**: Introduce a chunked operation payload (e.g. `type: "batch_op"` or string span insertion) that applies multiple contiguous characters in a single atomic network frame.

### Volatile Offline Buffer
- **Current Behavior**: Local edits created while disconnected are held in `offlineQueueRef` (in-memory React ref).
- **Risk**: If the user refreshes the browser page, navigates away, or closes the tab while disconnected, all un-synced edits are permanently lost.
- **Remediation**: Persist offline edits to browser `IndexedDB` or `localStorage` to ensure survival across page reloads.

---

## 4. Security & Access Control

### Zero Authentication & Plaintext Transport
- **Current Behavior**: Any client that can reach the WebSocket port can join any room ID without authentication, read document contents, or inject operations.
- **Risk**: Vulnerable to room sniffing, unauthorized document modifications, and denial of service.
- **Remediation**:
  - Add authentication handshake (token/JWT verification during HTTP upgrade).
  - Implement TLS/WSS termination (either natively or via reverse proxy like Nginx/Caddy/Cloudflare).
  - Add input validation and rate-limiting on message size and frame frequency in `Session.cpp`.

---

## Technical Debt Priority Matrix

| Priority | Issue | Location | Impact |
|---|---|---|---|
| **HIGH** | Lack of Character Deletion / Tombstones | `core/`, `client/src/engine/` | Users cannot delete text cooperatively |
| **HIGH** | Synchronous Full Disk Write on Every Keystroke | `server/DocumentHub.cpp` | Server I/O bottleneck under concurrent typing |
| **MEDIUM** | `FD_SETSIZE` 64 Socket Limit | `server/WebSocketServer.cpp` | Hard cap on maximum concurrent connections |
| **MEDIUM** | Granular Keystroke Loop on Text Paste | `client/src/App.jsx` | Network flooding on large text pastes |
| **LOW** | Multi-byte UTF-8 split across `char` | `core/include/collabpad/` | Potential corruption on concurrent non-ASCII edits |
| **LOW** | Unauthenticated Plaintext Sockets | `server/` | Zero access control or TLS encryption |
