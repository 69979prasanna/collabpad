# Testing Strategy & Test Suites

## Testing Overview

CollabPad employs a multi-tiered verification strategy spanning standalone C++ deterministic concurrency demos, end-to-end WebSocket protocol integration suites, and full automated regression suites covering network disconnects, server restarts, and persistence recovery.

```
+-------------------------------------------------------------------------------+
|                            Test Suite Hierarchy                               |
+-------------------------------------------------------------------------------+
  |
  +---> 1. Algorithmic Verification (C++ Native)
  |     - core/tests/phase1_demo.cpp (Sequential replica sync)
  |     - core/tests/phase2_demo.cpp (Concurrent RGA sibling ordering & buffering)
  |
  +---> 2. Wire Protocol & Networking (Python + websockets)
  |     - tests/test_server.py (RFC 6455 handshake, non-echo fanout, isolation)
  |
  +---> 3. Real-Time Collaboration & Presence (Python + websockets)
  |     - tests/test_collab_e2e.py (Concurrent typing, cursors, presence)
  |
  +---> 4. Persistence, Crash Recovery & Regression (Python + websockets)
        - tests/test_phase5.py (10-stage autonomous suite with process restarts)
```

---

## 1. Algorithmic Unit & Concurrency Demos (C++)

### Target 1: `collabpad_demo` (Phase 1)
- **Source**: `core/tests/phase1_demo.cpp`
- **Purpose**: Verifies that two independent replicas exchanging sequential insertion operations converge to identical document states.
- **Run Command**:
  ```powershell
  .\build\collabpad_demo.exe
  ```
- **Validation**: Prints replica texts and asserts convergence to `"Hello"`.

### Target 2: `collabpad_phase2_demo` (Phase 2)
- **Source**: `core/tests/phase2_demo.cpp`
- **Purpose**: Verifies the core RGA conflict resolution algorithm:
  1. Establishes common base text `"CAT"`.
  2. Simultaneously inserts `'R'` after `'A'` on Replica A (target: `"CART"`) and `'B'` after `'A'` on Replica B (target: `"CABT"`).
  3. Delivers operations in opposite arrival orders to both replicas.
  4. Validates that both replicas deterministically converge to `"CABRT"` based on `(clock, siteId)` ordering.
  5. Validates out-of-order operation arrival: operations with missing parent dependencies are safely buffered in `pendingOperations_` and applied once the parent arrives.
- **Run Command**:
  ```powershell
  .\build\collabpad_phase2_demo.exe
  ```

---

## 2. Server Integration Test (`test_server.py`)

- **Source**: `tests/test_server.py`
- **Focus**: Phase 3 WebSocket protocol conformance and room routing.
- **Prerequisites**: Server running on target port (default `8080`).
- **Execution**:
  ```powershell
  # Window 1: Start Server
  .\build\collabpad_server.exe 8080

  # Window 2: Execute Test
  python tests/test_server.py 8080
  ```
- **Test Steps**:
  1. Connects Client A and Client B to room `"demo"`.
  2. Connects Client C to separate room `"other"`.
  3. Sends operation from Client A.
  4. Asserts Client B receives the forwarded operation.
  5. Asserts Client A does **not** receive its own echoed message.
  6. Asserts Client C does **not** receive the operation (room isolation).
  7. Disconnects Client A and verifies the server remains responsive.

---

## 3. End-to-End Collaboration Test (`test_collab_e2e.py`)

- **Source**: `tests/test_collab_e2e.py`
- **Focus**: Phase 4 live collaboration features.
- **Prerequisites**: Server running on target port (default `8080`).
- **Execution**:
  ```powershell
  python tests/test_collab_e2e.py 8080
  ```
- **Test Scenarios**:
  1. Multi-client concurrent typing in the same room.
  2. Live cursor coordinate broadcasting and position delivery.
  3. Collaborator presence join/leave events.
  4. Strict room isolation preventing cross-room state contamination.

---

## 4. Comprehensive Regression Suite (`test_phase5.py`)

- **Source**: `tests/test_phase5.py`
- **Focus**: Autonomous verification of persistence, session recovery, and edge-case idempotency.
- **Execution**:
  ```powershell
  python tests/test_phase5.py 8095
  ```
  *(Launches on port 8095 to prevent collisions with active development servers; manages server process lifecycle autonomously).*

- **10 Core Verifications Executed**:
  1. **Document Creation & Save**: Creates document, generates operations, verifies JSON snapshot is written to disk.
  2. **Snapshot Load & State Verification**: Connects fresh client to room and verifies loaded text and character metadata.
  3. **Server Crash & Restart Recovery**: Force-terminates server process, restarts it, reconnects clients, and confirms document state is recovered intact from disk.
  4. **Concurrent Multi-Client Editing**: Simulates concurrent writers in the same room and validates non-echo message routing.
  5. **Strict Room Boundary Isolation**: Validates zero message leakage between distinct document rooms.
  6. **Ephemeral Presence Lifecycle**: Verifies join announcements and instant emission of leave/offline events upon client disconnect.
  7. **Room-Scoped Cursor Streaming**: Asserts cursor position payloads reach all peers in the room.
  8. **Disconnect & Reconnect Recovery**: Drops a client connection mid-session, rejoins, and verifies synchronization of missed operations via snapshot.
  9. **Operation Idempotency**: Feeds duplicate and already-seen operations; verifies replica remains identical and does not duplicate characters.
  10. **Deterministic RGA Replica Convergence**: Reconstructs both sides using RGA logic and asserts identical text convergence.

---

## Testing Matrix Summary

| Test Suite | Environment | Target Verified | Automated Lifecycle |
|---|---|---|---|
| `collabpad_demo` | C++ Binary | Basic replica sync | Self-contained executable |
| `collabpad_phase2_demo` | C++ Binary | RGA concurrency & dependency buffer | Self-contained executable |
| `test_server.py` | Python 3.7+ | WebSocket RFC 6455 & fanout routing | Requires running server |
| `test_collab_e2e.py` | Python 3.7+ | Typing, cursors, presence | Requires running server |
| `test_phase5.py` | Python 3.7+ | Full persistence, restarts, idempotency | Fully autonomous (spawns & kills server) |
