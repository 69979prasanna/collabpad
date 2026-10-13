# System Architecture

## Architecture Overview

CollabPad implements a distributed collaborative document editing architecture based on the **Replicated Growable Array (RGA)** Conflict-Free Replicated Data Type (CRDT), a native single-threaded C++ WebSocket relay server, and a reactive React frontend with dual-mode execution (JavaScript engine and C++ WebAssembly).

```
+-----------------------------------------------------------------------------------+
|                                 CollabPad Client                                  |
|   +------------------------------------+    +---------------------------------+   |
|   |          React 18 Frontend         |    |    Dual-Mode Engine Bridge      |   |
|   |  - App.jsx / Editor.jsx            |--->|  - Native JS RGA Implementation |   |
|   |  - Sidebar / CollaboratorsPanel    |    |  - Or WebAssembly C++ Replica   |   |
|   +-----------------+------------------+    +----------------+----------------+   |
|                     |                                        |                    |
|                     v                                        v                    |
|         +---------------------------------------------------------+               |
|         |                   useCollabSocket Hook                  |               |
|         | - Bounded Exponential Backoff    - Offline Op Buffer    |               |
|         | - Heartbeat & Presence Timer     - Cursor Throttle      |               |
|         +----------------------------+----------------------------+               |
+--------------------------------------|--------------------------------------------+
                                       | RFC 6455 Frames
                                       v
+-----------------------------------------------------------------------------------+
|                        CollabPad WebSocket Server (C++17)                         |
|   +---------------------------------------------------------------------------+   |
|   |                      Winsock2 select() Event Loop                         |   |
|   |  - Non-blocking I/O multiplexing                                          |   |
|   |  - RFC 6455 Handshake & Binary Frame Masking/Unmasking                    |   |
|   +-------------------------------------+-------------------------------------+   |
|                                         | Hand-off                                |
|                                         v                                         |
|   +---------------------------------------------------------------------------+   |
|   |                            DocumentHub                                    |   |
|   |  - Room Isolation map<string, DocumentRoom>                               |   |
|   |  - Non-echo message fanout                                                |   |
|   |  - Per-room authoritative collabpad::Replica state                        |   |
|   +----------------------+-----------------------------+----------------------+   |
+--------------------------|-----------------------------|--------------------------+
                           |                             |
                           v                             v
           +-------------------------------+   +------------------------------------+
           |    C++ CRDT Engine (Core)     |   |     Persistence Layer              |
           |  - RGA Sequence Tree          |   |  - DocumentRepository Abstraction  |
           |  - (clock, siteId) Ordering   |   |  - FileRepository (Atomic JSON)    |
           |  - Pending Dependency Buffer  |   |  - Crash / Reboot Recovery         |
           +-------------------------------+   +------------------------------------+
```

---

## 1. Core CRDT Mathematical Model: RGA (Replicated Growable Array)

The core editing engine solves concurrent insertions without central coordination or merge locks.

### Identifiers (`collabpad::CharId`)
Each character created in any replica is assigned a globally unique, immutable identifier:
```cpp
struct CharId {
    uint32_t siteId;  // Unique client/site integer identifier
    uint64_t clock;   // Lamport logical timestamp at generation time
};
```
- **Virtual Root**: `CharId{0, 0}` represents the start-of-document boundary.
- **Total Ordering Relation (`operator<`)**:
  1. Primary key: `clock` ascending.
  2. Secondary key (tie-breaker): `siteId` ascending.

### Tree Node Structure (`collabpad::Character`)
```cpp
struct Character {
    CharId id;        // Unique node ID
    CharId afterId;   // Parent node ID after which this character was inserted
    char value;       // Character value
};
```

### Deterministic Sibling Ordering Algorithm (`Sequence::findInsertIndex`)
When two replicas concurrently insert characters after the same parent node `afterId`:
1. Find visual index directly following `afterId`.
2. Traverse forward through children and descendants of `afterId`:
   - Inspect the direct child (sibling) of `afterId` that leads to the current character in the sequence.
   - If `newId > siblingId`: The new character has higher priority; insert before this entire sibling subtree.
   - If `newId < siblingId`: The new character has lower priority; skip this sibling and all of its descendants.
   - If traversal encounters a node that does not belong to `afterId`'s subtree, stop.
3. This guarantees that all replicas converge to the exact same visual string regardless of arrival order.

### Causal Dependency Resolution (`Replica::pendingOperations_`)
If network reordering causes an operation `op` to arrive whose parent `afterId` has not yet been delivered:
1. `Replica::applyOperation()` detects `!sequence_.contains(op.afterId)`.
2. The operation is placed into `pendingOperations_`.
3. Whenever any valid operation is inserted, `Replica::tryApplyPending()` iterates the pending buffer to check if dependencies have been satisfied.
4. Cascading unblocking continues iteratively until no further pending operations can be resolved.

---

## 2. Server Architecture & Concurrency Model

### Single-Threaded `select()` Event Loop
- **Design Decision**: Avoids Windows posix-thread issues in older MinGW runtimes by utilizing the standard Winsock2 `select()` socket multiplexing function.
- **Cycle**:
  1. Populates `fd_set` with listening socket and all active session sockets.
  2. Blocks on `select()` with a 100ms timeout to maintain responsive signal checking (`SIGINT` graceful shutdown).
  3. New connections: `accept()` socket and execute HTTP upgrade handshake (`WebSocketServer::doHandshake()`).
  4. Readable sockets: `Session::readFrame()` extracts text payload, unmasks bytes, and invokes `Session::handleMessage()`.
  5. Housekeeping: Iterates sessions to remove closed connections via `WebSocketServer::pruneDeadSessions()`.

### Multi-Tenancy & Room Isolation (`DocumentHub`)
- Documents are organized into distinct rooms identified by `docId`.
- Each `DocumentRoom` maintains:
  - `docId` & `title`.
  - An independent `std::unique_ptr<Replica> replica`.
  - A list of active `std::shared_ptr<Session> sessions`.
  - A table of active collaborator metadata `unordered_map<uint32_t, CollaboratorInfo>`.
  - Persistence dirty tracking flag `isDirty`.
- **Isolation Guarantee**: Messages (operations, presence, cursors) sent to room $A$ are never broadcast or leaked to room $B$.
- **Non-Echo Broadcast**: Messages are forwarded to all connected sessions in the room *excluding* the originating sender session.

---

## 3. Client Architecture

### Component Hierarchy
```
App.jsx (Root State, Site ID, Active Room, CRDT Engine Lifecycle)
├── WasmStatusBanner.jsx (WASM vs JS Fallback Indicator)
├── Toolbar.jsx (Document Title, Connection Status Pill, Collaborator Counter, Invite Button)
├── <main className="app-main">
│   ├── Sidebar.jsx (Workspaces, Shared Documents, Join Room Dialog)
│   ├── Editor.jsx (Dynamic Line Gutter, Monospace Surface, Floating Remote Cursors)
│   └── CollaboratorsPanel.jsx (Self Card, Collaborator List, Live Activity Log)
└── ShareModal.jsx (Room Copy Link / URL Sharing)
```

### Dual-Mode Engine Bridge (`engineBridge.js`)
- Exposes uniform API: `localInsertAfter()`, `applyOperation()`, `getText()`, `getIdAt()`, `size()`, `loadSnapshot()`.
- On startup, attempts asynchronous import of `window.CollabPadWasm`.
- When available: Instantiates `new window.CollabPadWasm.Replica(siteId)` and routes all calls to C++ WebAssembly.
- When unavailable: Uses native JavaScript class `Sequence` mirroring the exact C++ algorithm with identical behavior.

### Network Resiliency & State Reconciliation (`useCollabSocket.js`)
- **Bounded Exponential Backoff**:
  - Starts at 1000ms delay.
  - Multiplier $1.8 \times$ with random jitter (0–400ms).
  - Capped at a maximum ceiling of 15,000ms.
- **Offline Edit Buffering**:
  - Edits performed while disconnected are buffered in `offlineQueueRef`.
  - Upon reconnection and receiving `"type": "joined"`, all queued edits are flushed to the server.
- **Authoritative Snapshot Synchronization**:
  - On room join, the server provides the current authoritative snapshot.
  - The client merges missing operations without duplicating characters.
