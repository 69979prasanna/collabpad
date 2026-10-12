# CollabPad — Real-Time Collaborative Workspace

CollabPad is a high-performance, real-time collaborative code and text editor built with a C++ Conflict-Free Replicated Data Type (CRDT) engine, a custom native WebSocket server (RFC 6455 via Winsock2), durable document persistence, live room presence, and a modern React/Vite web application.

---

## Architecture Overview

```
+-------------------------------------------------------------------------------+
|                             CollabPad Web Client                              |
|   (React 18 + Vite, Dual-Mode Engine Bridge, Real-time Cursors & Presence)    |
|   (Bounded Exponential Backoff Reconnection, Offline Op Outgoing Buffer)      |
+---------------------------------------+---------------------------------------+
                                        |  WebSocket (RFC 6455 JSON Framing)
                                        v
+-------------------------------------------------------------------------------+
|                       CollabPad Native WebSocket Server                       |
|           (C++17, Winsock2 select() Event Loop, Room Isolation)               |
|            (Session Lifecycle, Ephemeral Presence & Cursor Router)            |
+-------------------+-----------------------------------+-----------------------+
                    |                                   |
                    v Operation Relay                   v Snapshot State
+---------------------------------------+   +-----------------------------------+
|        C++ CRDT Engine (Core)         |   |    Document Persistence Layer     |
| (RGA Sequence, Lamport Timestamps,    |   | (DocumentRepository Abstraction,  |
|  Pending Dependency Buffer, Snapshot) |   |  FileRepository Atomic JSON I/O)  |
+---------------------------------------+   +-----------------------------------+
```

### Components

1. **Core CRDT Engine (`core/include/collabpad/`)**:
   - Header-only C++ library implementing the Replicated Growable Array (RGA) CRDT.
   - Deterministic concurrency resolution based on `(clock, siteId)` ordering.
   - Out-of-order delivery tolerance via an automatic pending operation dependency buffer.
   - Complete snapshot serialization (`DocumentSnapshot.hpp`) preserving full CRDT metadata (CharIds, parent IDs, values, and Lamport clocks).

2. **WebSocket Server (`server/`)**:
   - Native C++ single-threaded `select()`-based event loop using Windows Sockets (`ws2_32`).
   - Implements full RFC 6455 handshake, frame masking/unmasking, and room isolation (`DocumentHub`).
   - Tracks session lifecycle with immediate leave/offline event broadcasts on client disconnect.

3. **Persistence Engine (`server/DocumentRepository.hpp`, `server/FileRepository.cpp`)**:
   - Clean repository abstraction with `save`, `load`, `exists`, and `listDocuments`.
   - `FileRepository` provides safe file I/O with atomic temporary-file writes (`.tmp` to `.json` replace) to prevent data corruption during fast concurrent operations.
   - Durable restart recovery: documents are safely reloaded into memory when clients rejoin after a server reboot.

4. **Web Client (`client/`)**:
   - Modern dark engineering-oriented user interface.
   - Live editable code and text editor with dynamic line numbering.
   - Real-time collaborator cursors, presence indicators (Active / Typing / Idle), and activity feed.
   - Multi-room and multi-document workspace management.
   - Bounded exponential backoff reconnection with offline edit buffering.

5. **WebAssembly Integration (`wasm/`)**:
   - Emscripten Embind bindings (`wasm/bindings.cpp`) exposing `Replica`, `Sequence`, `CharId`, and `InsertionOp`.
   - Dual-mode frontend bridge that dynamically drives the compiled WebAssembly binary when present, with automatic zero-configuration JavaScript engine fallback.

---

## Prerequisites & Development Environment

- **Operating System**: Windows 10/11
- **C++ Toolchain**: MinGW GCC 6.3.0+ & CMake 3.15+
- **Python**: Python 3.7+ with `websockets` (`pip install websockets`)
- **Node.js**: Node.js 18+ and npm

---

## Quick Start & Verification

### 1. Build the Native Targets (C++)

Open PowerShell in the repository root:
```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

This builds:
- `build/collabpad_demo.exe` (Phase 1 index-based CRDT demo)
- `build/collabpad_phase2_demo.exe` (Phase 2 RGA concurrent engine demo)
- `build/collabpad_server.exe` (Phase 3–5 native WebSocket server with persistence)

---

### 2. Run Verification Demos

#### Phase 1 Demo
```powershell
.\build\collabpad_demo.exe
```
*Verifies basic replica synchronization and convergence.*

#### Phase 2 Demo
```powershell
.\build\collabpad_phase2_demo.exe
```
*Verifies RGA concurrent insertions after identical parents and buffering of out-of-order operations.*

---

### 3. Run Automated Test Suites

#### Phase 3 Server Test
Start the server in one window:
```powershell
.\build\collabpad_server.exe 8080
```
Run the test in a second window:
```powershell
python tests/test_server.py 8080
```
*Verifies client connections, room routing, non-echo broadcasting, room isolation, and disconnect recovery.*

#### Phase 4 End-to-End Collaboration Test
```powershell
python tests/test_collab_e2e.py 8080
```
*Verifies multi-client concurrent typing, cursor position broadcasting, presence tracking, and room boundaries.*

#### Phase 5 Persistence, Presence, and Session Recovery Test
```powershell
python tests/test_phase5.py 8095
```
*Autonomously launches and verifies:*
1. Creating and saving documents with stable IDs.
2. Loading saved document snapshots.
3. Server restart and document recovery.
4. Two clients concurrently editing the same room.
5. Strict room isolation between different document IDs.
6. Live presence join and immediate leave/offline event broadcast on disconnect.
7. Cursor position delivery scoped to active rooms.
8. Client disconnection, reconnection, and missed-operation recovery.
9. Duplicate and out-of-order operation idempotency.
10. Deterministic RGA replica convergence (`(clock, siteId)` ordering).

---

### 4. Run the Collaborative Web Client

In the `client/` directory:
```powershell
cd client
npm install
npm run dev
```

Open your browser at `http://localhost:3000`.

To test real-time collaboration:
1. Open `http://localhost:3000` in two separate browser tabs or an Incognito window.
2. Both tabs will automatically connect to the same room (`demo`).
3. Type text or code in one tab — edits and live collaborator cursors synchronize instantly!
4. Create or join another room to verify room isolation.
5. Kill and restart the server — refresh or reconnect the client and observe that documents remain intact!

---

### 5. Architectural Boundaries & Offline Editing Limitations

- **Durable Document Persistence**: Document state is saved to disk on every confirmed operation using atomic temporary-file writes (`FileRepository`). Storage failures are logged visibly to standard error rather than failing silently.
- **Session Recovery & Missed Operations**: When a client reconnects, the server transmits the authoritative `DocumentSnapshot`. The client's CRDT engine idempotently merges any missing characters without duplicating nodes.
- **Offline Editing Scope**: Local edits made during temporary disconnections are safely buffered in memory in an outgoing queue. Once the WebSocket connection re-establishes, the queue is automatically flushed to the server.
- **Limitations**: In this phase, offline editing relies on short-to-medium-term network resilience. If a client remains offline for prolonged periods while conflicting collaborators perform extensive structural rewrites without reconnecting, local memory buffers will hold unacknowledged edits; full offline distributed branching and asynchronous three-way branch merges are outside the scope of Phase 5.
