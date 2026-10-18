# CollabPad Architecture & System Design Document (DESIGN.md)

**Status**: Active / Baseline  
**Version**: 0.2.0  
**Evidence Standard**: Evidence-Based (VERIFIED / REPORTED / PLANNED / UNKNOWN)

---

## 1. High-Level Architecture Overview

CollabPad is structured as a modular, layered system with clear separation of concerns across native backend services, core CRDT algorithms, data persistence, and web client interfaces.

```
+-----------------------------------------------------------------------------------+
|                               CollabPad Web Client                                |
|   - React 18 + Vite (UI Surface, Editor, Cursors, Presence, Document Tree)       |
|   - Dual-Mode Engine Bridge: [WebAssembly (WASM)] <---> [JavaScript Fallback]    |
|   - useCollabSocket: Bounded Backoff, Offline Operation Queue, Heartbeats         |
+-----------------------------------------+-----------------------------------------+
                                          |
                                          | WebSocket (RFC 6455 Wire Protocol)
                                          | JSON Framing over TCP (Winsock2)
                                          v
+-----------------------------------------------------------------------------------+
|                        CollabPad Native WebSocket Server                          |
|   - WebSocketServer (Winsock2 select() Single-Threaded Event Loop)                |
|   - RFC 6455 Framing & Masking Parser                                             |
|   - Session Manager (Connection Lifecycle & Frame Serialization)                  |
|   - DocumentHub (Room Multiplexing, Non-Echo Operation Fanout, Presence Router)   |
+---------------------+---------------------------------------+---------------------+
                      |                                       |
                      v Local CRDT Mutation                   v Durable State Snapshots
+-------------------------------------+   +-----------------------------------------+
|     C++ CRDT Engine (Core)          |   |       Document Persistence Layer        |
|  - RGA Sequence Tree                |   |   - DocumentRepository (Interface)      |
|  - Lamport Logical Clock            |   |   - FileRepository (Atomic .tmp rename) |
|  - InsertionOp & DeletionOp         |   |   - DocumentSnapshot Serialization      |
|  - Pending Operation & Delete Buffer|   |   - Periodic/Vacancy Flush (Phase 7)    |
|  - Tombstone Tracking (isDeleted)   |   +-----------------------------------------+
+-------------------------------------+
```

---

## 2. Component Breakdown & Source Tracing

### 2.1 Core CRDT Engine (`core/include/collabpad/`)

The core engine is a **header-only C++17 library** that is strictly decoupled from network sockets, file systems, and UI frameworks.

#### Data Structures:
- **`CharId` (`core/include/collabpad/CharId.hpp`)**:
  - Struct representing a character identity: `{ uint32_t siteId; uint64_t clock; }`.
  - Special `CharId::root()` is `{ siteId: 0, clock: 0 }`, representing the beginning of the sequence.
  - Relational operators (`==`, `!=`, `<`, `>`): ordered by `clock` ascending; ties broken by `siteId` ascending.
- **`Character` (`core/include/collabpad/Sequence.hpp`)**:
  - Struct: `{ CharId id; CharId afterId; char value; bool isDeleted; }`.
  - Contains parent pointer `afterId` and tombstone flag `isDeleted`.
- **`Sequence` (`core/include/collabpad/Sequence.hpp`)**:
  - Stores characters in a dense contiguous vector `std::vector<Character> elements_`.
  - `findInsertIndex(newId, afterId)`: walks siblings after `afterId`. If an existing sibling branch has a lower `CharId` than `newId`, `newId` is placed before it. If higher, `newId` skips that sibling and all its descendants.
  - `getText()`: accumulates `ch.value` for all nodes where `!ch.isDeleted`.
  - `visibleSize()`: returns count of non-deleted characters.
  - `getIdAt(visualIndex)`: translates 0-based visible cursor index to the underlying `CharId`.
  - `findIndex(id)`: returns 0-based visible index, or `-1` if deleted or absent.
  - `markDeleted(id)`: sets `isDeleted = true` idempotently.
  - `loadCharacters(chars)`: sets underlying vector directly for snapshot hydration.
- **`LamportClock` (`core/include/collabpad/LamportClock.hpp`)**:
  - Monotonic clock: `tick()` increments and returns the new value; `update(remoteTime)` sets `time_ = max(time_, remoteTime) + 1`.
- **`Operation` (`core/include/collabpad/Operation.hpp`)**:
  - `InsertionOp`: `{ CharId id; CharId afterId; char value; }`.
  - `DeletionOp`: `{ CharId targetId; uint32_t siteId; uint64_t clock; }`.
- **`Replica` (`core/include/collabpad/Replica.hpp`)**:
  - Represents a peer or server node with assigned `siteId_`.
  - `localInsertAfter(afterId, value)`: advances clock, inserts node, returns `InsertionOp`.
  - `localDelete(targetId)`: advances clock, marks tombstone, returns `DeletionOp`.
  - `localDeleteAt(visualIndex)`: resolves visual index to `CharId` and delegates to `localDelete`.
  - `applyOperation(InsertionOp)`: updates clock; if `afterId` is not in sequence, buffers in `pendingOperations_`; otherwise inserts into sequence and triggers `tryApplyPending()` and `checkPendingDeletion()`.
  - `applyDelete(DeletionOp)`: updates clock; if `targetId` is not present, buffers in `pendingDeletions_`; otherwise calls `sequence_.markDeleted()`.
  - `createSnapshot()` / `loadSnapshot()`: exports or restores state including active `pendingDeletions_`.

### 2.2 WebSocket Relay Server (`server/`)

Single-threaded, non-blocking multiplexer built directly on Windows Sockets 2 (`ws2_32`).

- **`WebSocketServer` (`server/WebSocketServer.hpp`, `.cpp`)**:
  - Initializes Winsock2 (`WSAStartup`) and binds a non-blocking listening socket.
  - Main loop `run()` executes `select()` with a 100ms timeout (`timeval { 0, 100000 }`).
  - Dispatches new connections to `Session` instances and reads incoming socket frames.
  - Housekeeping: periodically cleans up dead sessions via `pruneDeadSessions()` and `hub_->prune()`.
- **`Session` (`server/Session.hpp`, `.cpp`)**:
  - Manages raw socket buffer, performs RFC 6455 handshake (reading `Sec-WebSocket-Key`, computing SHA-1 with RFC 6455 magic GUID `258EAFA5-E914-47DA-95CA-C5AB0DC85B11`, and sending HTTP 101 Switching Protocols response).
  - Unmasks incoming client frames (XOR mask parsing).
  - Handles WebSocket OpCodes: `0x1` (Text frame), `0x8` (Close frame), `0x9` (Ping), `0xA` (Pong).
  - Sends outgoing text frames (unmasked server-to-client framing per RFC 6455 §5.1).
- **`DocumentHub` (`server/DocumentHub.hpp`, `.cpp`)**:
  - Maintains `std::unordered_map<std::string, DocumentRoom> rooms_`.
  - Each `DocumentRoom` contains:
    - `std::string docId; std::string title;`
    - `std::unique_ptr<Replica> replica;` (Server siteId = 0)
    - `std::vector<std::shared_ptr<Session>> sessions;`
    - `std::unordered_map<uint32_t, CollaboratorInfo> collaborators;`
    - `bool isDirty;`
    - `uint64_t firstDirtyTimeMs; uint64_t lastEditTimeMs;` (Phase 7 timing fields)
  - `join(docId, session, siteId, name)`: registers session in room, records collaborator presence, and returns the authoritative `DocumentSnapshot`.
  - `leave(docId, sessionId)`: unregisters session and broadcasts offline presence to peers.
  - `broadcast(docId, senderSessionId, message)`: relays messages to all other alive sessions in the room (non-echo fanout). If message is an `op`, calls `applyOpJson()`.
  - `applyOpJson(room, json)`: parses `InsertionOp` or `DeletionOp`, executes on server replica, and persists to repository.

### 2.3 Persistence Layer (`server/DocumentRepository.hpp`, `FileRepository.cpp`)

- **`DocumentSnapshot` (`core/include/collabpad/DocumentSnapshot.hpp`)**:
  - Serializes document replica state to JSON string:
    ```json
    {
      "docId": "demo",
      "title": "Live Collab Demo",
      "clock": 12,
      "characters": [
        {"id":{"siteId":1,"clock":1},"afterId":{"siteId":0,"clock":0},"value":"H","deleted":false},
        {"id":{"siteId":1,"clock":2},"afterId":{"siteId":1,"clock":1},"value":"E","deleted":false}
      ],
      "pendingDeletions": []
    }
    ```
  - Parses JSON with backwards-compatibility: missing `"deleted"` defaults to `false`; missing `"pendingDeletions"` defaults to empty array.
- **`FileRepository` (`server/FileRepository.hpp`, `.cpp`)**:
  - Implements `DocumentRepository`.
  - `save(snapshot)`:
    1. Writes complete snapshot JSON into a temporary file: `<storageDir>/<docId>.json.tmp`.
    2. Calls Win32 `MoveFileExA(tmpPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.
    3. Guarantees atomic replacement: if the power fails or server crashes mid-write, the existing `.json` file remains uncorrupted.
  - `load(docId, snapshot)`: loads `<docId>.json` and deserializes into `DocumentSnapshot`.

### 2.4 Dual-Mode Engine Bridge & Web Client (`client/`)

- **`engineBridge.js` (`client/src/engine/engineBridge.js`)**:
  - Implements identical RGA logic in JavaScript: `CharId`, `Character`, `Sequence`, and `EngineBridge`.
  - Startup check: probes `window.CollabPadWasm`. If compiled WebAssembly is available, instantiates `new mod.Replica(siteId)` and delegates operations to C++; otherwise runs the native JavaScript RGA engine.
  - Maintains `sequence`, `pendingOps`, and `pendingDeletions`.
- **`useCollabSocket.js` (`client/src/hooks/useCollabSocket.js`)**:
  - WebSocket lifecycle management.
  - Connection backoff: initial delay 1,000ms, max 15,000ms, backoff factor 1.5 with random jitter.
  - Queues outgoing operations during disconnects in `pendingOpsQueueRef`; flushes queue upon reconnect.
  - Cursors throttled to 50ms interval.
- **`Editor.jsx` (`client/src/components/Editor.jsx`)**:
  - Monospace text area synchronized with line number gutter scroll.
  - Floating remote cursor banners showing collaborator names and colors.
- **`App.jsx` (`client/src/App.jsx`)**:
  - Computes text diffs on `textarea` changes using common prefix/suffix detection.
  - Deletions: maps deleted characters from end to start to `getIdAt()`, calls `localDelete()`, and dispatches delete ops.
  - Insertions: calls `localInsertAfter()`, updates cursor parent pointer, and dispatches insert ops.

---

## 3. Communication Protocol Contracts

All WebSocket messages are encoded as UTF-8 JSON text frames.

### 3.1 Client to Server Messages

#### Join Room:
```json
{
  "type": "join",
  "docId": "demo",
  "siteId": 101,
  "name": "Alice"
}
```

#### Insertion Operation:
```json
{
  "type": "op",
  "docId": "demo",
  "siteId": 101,
  "op": {
    "id": { "siteId": 101, "clock": 5 },
    "afterId": { "siteId": 101, "clock": 4 },
    "value": "x"
  }
}
```

#### Deletion Operation:
```json
{
  "type": "op",
  "docId": "demo",
  "siteId": 101,
  "op": {
    "type": "delete",
    "siteId": 101,
    "clock": 6,
    "targetId": { "siteId": 101, "clock": 4 }
  }
}
```

#### Cursor Position:
```json
{
  "type": "cursor",
  "docId": "demo",
  "siteId": 101,
  "name": "Alice",
  "color": "#6366f1",
  "cursor": { "line": 12, "col": 4, "index": 142 }
}
```

#### Presence Heartbeat:
```json
{
  "type": "presence",
  "docId": "demo",
  "siteId": 101,
  "name": "Alice",
  "status": "editing"
}
```

### 3.2 Server to Client Messages

#### Join Acknowledgment & Snapshot Synchronization:
```json
{
  "type": "joined",
  "docId": "demo",
  "snapshot": {
    "docId": "demo",
    "title": "Live Collab Demo",
    "clock": 6,
    "characters": [
      {
        "id": { "siteId": 101, "clock": 4 },
        "afterId": { "siteId": 0, "clock": 0 },
        "value": "H",
        "deleted": false
      }
    ],
    "pendingDeletions": []
  }
}
```

#### Operation Fanout (Non-Echo Relay):
Identical payload to the client's `type: "op"` message, broadcast to all other sessions in the room.

#### Presence Notification:
```json
{
  "type": "presence",
  "docId": "demo",
  "siteId": 101,
  "name": "Alice",
  "status": "offline",
  "event": "leave"
}
```

---

## 4. Invariant Contracts & Protected Interfaces

The following public APIs, data structures, and behaviors must not be modified or broken without explicit architectural approval:

1. **RGA Deterministic Ordering Rule**:
   - For siblings sharing parent `afterId`, node with higher `(clock, siteId)` MUST precede node with lower `(clock, siteId)`.
   - Modifying this rule breaks convergence between client and server replicas.
2. **`CharId::root()` Contract**:
   - `CharId(0, 0)` is strictly reserved as the virtual root representing the start of document. No local character may ever receive clock 0 or siteId 0.
3. **Tombstone Preservation Contract**:
   - `markDeleted()` MUST NOT remove the element from `Sequence::elements_`.
   - `Sequence::findInsertIndex()` MUST continue traversing deleted nodes when finding descendant boundaries.
4. **Wire Protocol Compatibility**:
   - The JSON keys (`"type"`, `"docId"`, `"siteId"`, `"op"`, `"id"`, `"afterId"`, `"targetId"`, `"value"`, `"cursor"`, `"snapshot"`) must remain stable.
5. **Atomic Storage Format**:
   - The `.tmp` file writing followed by atomic `MoveFileExA` replace must remain the only mechanism for modifying snapshot files. Direct in-place writes to live files are strictly forbidden.

---

## 5. Persistence Lifecycle & Phase 7 Planned State

### Current Implementation State (VERIFIED):
- In `server/DocumentHub.cpp`: Every incoming insert or delete operation triggers an immediate synchronous call:
  ```cpp
  DocumentSnapshot snap = room.replica->createSnapshot(room.docId, room.title);
  repository_->save(snap);
  room.isDirty = false;
  ```
- This guarantees zero data loss on server crash, but introduces disk I/O latency (~1ms–2ms per keystroke) on the single-threaded WebSocket event loop.

### Approved Phase 7 Architecture (PLANNED):
- **Decoupled dirty marking**: Keystroke operations set `room.isDirty = true` and update `firstDirtyTimeMs` and `lastEditTimeMs`. No synchronous disk write occurs in `applyOpJson`.
- **Event-Loop Debounce Flusher (`flushDirty`)**:
  - `WebSocketServer::run()` invokes `hub_->flushDirty(nowMs)` at the end of each `select()` loop iteration.
  - Flush conditions: `(nowMs - room.lastEditTimeMs >= 1000)` OR `(nowMs - room.firstDirtyTimeMs >= 3000)`.
- **Room Vacancy Flush**: When `DocumentHub::leave()` removes the last active session from a room, an immediate save is triggered if the room is dirty.
- **Graceful Shutdown**: Win32 `SetConsoleCtrlHandler` and `hub_->flushAll()` ensure dirty documents are committed before process termination.
- **Evidence Level**:
  - Declarations in `DocumentHub.hpp`: **VERIFIED**.
  - Implementation in `DocumentHub.cpp` / `WebSocketServer.cpp`: **PLANNED / UNIMPLEMENTED**.

---

## 6. Build and Test Workflows

### Native C++ Targets:
```powershell
# Build all targets using MinGW Makefiles
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```
- Targets produced:
  - `build/collabpad_demo.exe` (Phase 1 Sequential CRDT demo)
  - `build/collabpad_phase2_demo.exe` (Phase 2 RGA Concurrency & Out-of-Order demo)
  - `build/collabpad_phase6_demo.exe` (Phase 6 RGA Deletion & Tombstone demo)
  - `build/collabpad_server.exe` (Native WebSocket Server)

### Web Client Targets:
```powershell
cd client
npm install
npm run build   # Production Vite bundle in client/dist
npm run dev     # Local Vite dev server on http://localhost:3000
```

### Automated Integration & Verification Suites:
```powershell
# 1. Phase 6 End-to-End Deletion & Recovery Suite
python tests/test_deletion.py 8096

# 2. Phase 5 10-Stage Full Integration Suite
python tests/test_phase5.py 8095

# 3. Phase 3 WebSocket Wire Framing Suite (requires running server)
python tests/test_server.py 8080

# 4. Phase 4 Collaboration & Multi-Room E2E Suite (requires running server)
python tests/test_collab_e2e.py 8080
```

---

## 7. Known Architectural Limitations & Security Boundaries

1. **FD_SETSIZE Limit (64 Sockets)**: Standard Windows Winsock2 `fd_set` limits the `select()` loop to 64 simultaneous connections. Scaling beyond 64 requires `WSAEventSelect` or IOCP (planned for Phase 10).
2. **Single-Byte Characters**: Node `Character.value` is currently stored as a single C++ `char`. Multi-byte UTF-8 code points (such as emojis or non-ASCII scripts) can be split if pasted or concurrently edited (planned for Phase 9).
3. **Open Access Control**: Rooms are joined solely by `docId` with no authentication tokens or permissions (planned for Phase 10).
4. **In-Memory Room Growth**: Rooms created during server uptime remain in memory in `DocumentHub::rooms_` until the process is restarted.
