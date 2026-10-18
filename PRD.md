# Product Requirements Document (PRD) — CollabPad

**Document Status**: Active / Baseline  
**Version**: 0.2.0 (Milestone 2 In-Progress)  
**Last Updated**: October 2026  
**Evidence Standard**: Evidence-Based (VERIFIED / REPORTED / PLANNED / UNKNOWN)

---

## 1. Product Vision & Value Proposition

CollabPad is a high-performance, real-time collaborative text and code workspace engineered with a mathematically sound Conflict-Free Replicated Data Type (CRDT) engine, a zero-dependency native C++ WebSocket server, crash-resilient persistence, and a modern dual-mode web client.

### Core Problem Solved
Traditional collaborative platforms depend on complex third-party SaaS infrastructure, cloud message brokers, or heavy Operational Transformation (OT) servers that require continuous central coordination. CollabPad delivers low-latency, deterministic multi-user collaboration with:
- Zero external server runtime dependencies (native C++ on Winsock2).
- Deterministic conflict resolution via Replicated Growable Array (RGA).
- Resilience to out-of-order networks, abrupt client disconnections, and server restarts.
- Flexible client execution: compiled WebAssembly speed with instant JavaScript fallback.

---

## 2. Intended Users & Target Personas

1. **Software Engineers & Technical Teams**: Require low-latency concurrent code editing, line-number gutter synchronization, remote collaborator presence flags, and local edit buffering.
2. **Systems & Embedded Developers**: Value a zero-dependency native server (`collabpad_server.exe`) that runs directly on Windows without Docker, Redis, or Node.js backends.
3. **Distributed Systems Researchers**: Inspect and test formal CRDT convergence properties, Lamport logical clocks, out-of-order causal dependency buffering, and tombstone tracking.

---

## 3. Core Use Cases & User Journeys

### UC-1: Real-Time Multi-User Collaborative Editing
- **Journey**: Users open the web client in modern browsers, join the same document room (e.g., `demo`), and edit simultaneously.
- **Outcome**: Every keystroke (insertion or backspace/delete) propagates across all connected peers. Deterministic ordering guarantees that all replicas display the identical text.

### UC-2: Reconnection & Offline Recovery
- **Journey**: A collaborator experiences temporary network disconnects or closes their tab. They continue typing locally while disconnected.
- **Outcome**: The web client buffers outgoing edits. Upon network restoration, the client fetches the authoritative document snapshot, merges missing operations without duplication, flushes queued local edits, and converges.

### UC-3: Durable Workspace Recovery Across Server Restarts
- **Journey**: The native server process terminates unexpectedly or is restarted for maintenance.
- **Outcome**: Document snapshots saved to disk are safely reloaded on boot. Active tombstones and character sequences persist intact. No deleted characters reappear ("ghost characters").

---

## 4. Functional Requirements

### 4.1 Core CRDT Engine (RGA)
- **FR-CRDT-01 (Lamport Clocks)**: Maintain a monotonically increasing Lamport logical clock on every replica. Clock must advance on local operations (`tick()`) and synchronize with received operations (`update(remoteClock)`).  
  *Status*: **VERIFIED** (tested in `collabpad_demo`, `collabpad_phase2_demo`, `collabpad_phase6_demo`).
- **FR-CRDT-02 (Unique Identity)**: Every character node must have a globally unique `CharId(siteId, clock)`.  
  *Status*: **VERIFIED** (`core/include/collabpad/CharId.hpp`).
- **FR-CRDT-03 (Deterministic Concurrency)**: Sibling insertions after an identical parent must be ordered deterministically: higher Lamport clock takes precedence; ties are broken by higher `siteId`.  
  *Status*: **VERIFIED** (`core/tests/phase2_demo.cpp`, `tests/test_phase5.py`).
- **FR-CRDT-04 (Causal Dependency Buffering)**: Operations arriving before their parent character must be buffered in `pendingOperations_` and applied automatically once the parent is inserted.  
  *Status*: **VERIFIED** (`core/tests/phase2_demo.cpp`, `tests/test_phase5.py`).
- **FR-CRDT-05 (Deletion via Tombstones)**: Character deletion must mark nodes with `isDeleted = true` (tombstone) rather than removing them from the sequence tree, preserving causal history for concurrent edits.  
  *Status*: **VERIFIED** (`core/include/collabpad/Sequence.hpp`, `tests/test_deletion.py`).
- **FR-CRDT-06 (Out-of-Order Deletions)**: If a `DeletionOp` arrives before its target character, it must be buffered in `pendingDeletions_` and immediately tombstoned upon character insertion.  
  *Status*: **VERIFIED** (`core/tests/phase6_demo.cpp`, `tests/test_deletion.py`).
- **FR-CRDT-07 (Operation Idempotency)**: Redelivering duplicate insertions or deletions must be a no-op and never duplicate nodes or alter text.  
  *Status*: **VERIFIED** (`tests/test_phase5.py`, `tests/test_deletion.py`).
- **FR-CRDT-08 (UTF-8 Multi-Byte Code Points)**: Support multi-byte characters and emojis without byte splitting.  
  *Status*: **PLANNED** (Phase 9). Currently nodes store `char`.

### 4.2 Server & WebSocket Relay
- **FR-SRV-01 (RFC 6455 Framing)**: Native implementation of WebSocket handshake, SHA-1 digest, Base64 response, frame unmasking, and text framing over Winsock2.  
  *Status*: **VERIFIED** (`server/WebSocketServer.cpp`, `server/Session.cpp`, `tests/test_server.py`).
- **FR-SRV-02 (Room Routing & Multi-Tenancy)**: Isolate documents by `docId`. Operations in one room must never broadcast to another room.  
  *Status*: **VERIFIED** (`tests/test_server.py`, `tests/test_phase5.py`).
- **FR-SRV-03 (Non-Echo Broadcasting)**: Server forwards client operations to all other clients in the room, omitting the sender to avoid echo loops.  
  *Status*: **VERIFIED** (`tests/test_server.py`, `tests/test_collab_e2e.py`).
- **FR-SRV-04 (Session Lifecycle & Ephemeral Presence)**: Broadcast join and immediate offline/leave presence notifications when clients connect or disconnect.  
  *Status*: **VERIFIED** (`tests/test_phase5.py`, `server/DocumentHub.cpp`).
- **FR-SRV-05 (High-Concurrency Socket Polling)**: Support concurrent socket connections beyond standard Win32 64 `FD_SETSIZE`.  
  *Status*: **PLANNED** (Phase 10).

### 4.3 Persistence & Storage
- **FR-PST-01 (Repository Interface)**: Abstract `DocumentRepository` defining `save`, `load`, `exists`, and `listDocuments`.  
  *Status*: **VERIFIED** (`server/DocumentRepository.hpp`).
- **FR-PST-02 (Atomic File Replacement)**: Persist snapshots using temporary files (`.tmp`) atomically replaced via `MoveFileExA` to prevent file corruption during crashes.  
  *Status*: **VERIFIED** (`server/FileRepository.cpp`, `tests/test_phase5.py`).
- **FR-PST-03 (Complete CRDT Snapshotting)**: JSON snapshots must capture docId, title, clock, full character list with parents, values, tombstone flags, and pending deletions.  
  *Status*: **VERIFIED** (`core/include/collabpad/DocumentSnapshot.hpp`, `tests/test_deletion.py`).
- **FR-PST-04 (Legacy Snapshot Compatibility)**: Loading snapshots created prior to Phase 6 (without `deleted` or `pendingDeletions`) must default `isDeleted` to false and load successfully.  
  *Status*: **VERIFIED** (`core/tests/phase6_demo.cpp` Case 6, `tests/test_deletion.py` Case 7).
- **FR-PST-05 (Debounced / Asynchronous Persistence)**: Replace per-keystroke synchronous disk writes with debounced flushing (1s idle, 3s max flush interval, immediate flush on room vacancy).  
  *Status*: **PLANNED** (Phase 7 — approved plan in `.planning/phases/07-asynchronous-debounced-persistence/PLAN.md`; header declared, implementation pending).

### 4.4 Web Client & User Interface
- **FR-CLI-01 (Code & Text Editor Surface)**: Monospace text surface with dynamic line numbers, scroll synchronization, and tab support.  
  *Status*: **VERIFIED** (`client/src/components/Editor.jsx`).
- **FR-CLI-02 (Live Remote Cursors)**: Real-time visual collaborator flags showing cursor line/column and user tag.  
  *Status*: **VERIFIED** (`client/src/components/Editor.jsx`, `tests/test_collab_e2e.py`).
- **FR-CLI-03 (Presence & Activity Log)**: Display active/idle/typing status and room participant list.  
  *Status*: **VERIFIED** (`client/src/components/CollaboratorsPanel.jsx`).
- **FR-CLI-04 (Network Backoff & Edit Buffering)**: Bounded exponential backoff reconnection with jitter (1s–15s) and local offline edit buffering.  
  *Status*: **VERIFIED** (`client/src/hooks/useCollabSocket.js`).
- **FR-CLI-05 (Batch / Paste Operations)**: Atomic batch insertions for multi-character clipboard paste events.  
  *Status*: **PLANNED** (Phase 8).

### 4.5 WebAssembly & Dual-Mode Engine Bridge
- **FR-WASM-01 (Embind Bindings)**: Export `CharId`, `InsertionOp`, `DeletionOp`, `Sequence`, and `Replica` to WebAssembly.  
  *Status*: **VERIFIED** (`wasm/bindings.cpp`).
- **FR-WASM-02 (Dual-Mode Execution)**: Client probes for `CollabPadWasm`; automatically runs compiled C++ Wasm when present, transparently falling back to native JavaScript RGA if absent.  
  *Status*: **VERIFIED** (`client/src/engine/engineBridge.js`, `client/src/components/WasmStatusBanner.jsx`).

---

## 5. Non-Functional Requirements

| Category | Requirement | Target Standard | Current Status |
|---|---|---|---|
| **Correctness** | Strong Eventual Consistency (SEC) | Replicas receiving the same operations in any causal order converge to identical text. | **VERIFIED** (`test_phase5.py`, `test_deletion.py`) |
| **Durability** | Crash-Safe Snapshot Writing | Zero file truncation on sudden process termination; atomic rename via Win32 `MoveFileExA`. | **VERIFIED** (`FileRepository.cpp`) |
| **Performance** | Keystroke Throughput | Web relay must sustain >200 ops/sec without disk I/O bottlenecks. | **PLANNED** (Target for Phase 7 debounced flush; currently per-op synchronous writes limit throughput) |
| **Latency** | Local Edit Echo | Zero local input lag (optimistic local insert/delete before WebSocket dispatch). | **VERIFIED** (`App.jsx`) |
| **Reliability** | Disconnect Recovery | Full snapshot catch-up on reconnect; out-of-order delivery tolerance. | **VERIFIED** (`useCollabSocket.js`, `test_phase5.py`) |
| **Security** | Secret Handling | Zero credentials or secret environment tokens in repo, code, or logs. | **VERIFIED** (Audit confirmed no secrets or private keys stored) |
| **Security** | Room Access Control | Document rooms currently open by `docId` without token auth. | **PLANNED** (Phase 10) |
| **Compatibility**| Backward-Compatible Serialization | New versions must load legacy document JSON snapshots without failure. | **VERIFIED** (`test_deletion.py` Case 7) |

---

## 6. Explicit Non-Goals

1. **Rich Text / WYSIWYG Formatting**: CollabPad is strictly a plain text and source code collaborative editor. Bold, italics, markdown AST blocks, and embedded media are out of scope.
2. **Arbitrary Distributed Branching & 3-Way Git Merging**: CollabPad maintains an active real-time linear document sequence. Git-style branch diverging and offline merge conflicts lasting days are non-goals.
3. **Third-Party SaaS Relay Services**: No integration with Pusher, Firebase, AWS AppSync, or external message queues. The server must remain fully self-contained.
4. **Multi-Threaded Server Engine**: The server architecture relies on a single-threaded non-blocking `select()` event loop to prevent concurrency bugs and MinGW thread model crashes. Multi-threaded worker pools for networking are an explicit non-goal.

---

## 7. Acceptance Criteria for Critical Features

### AC-1: Character Deletion & Tombstone Convergence (Phase 6)
- Deleting characters emits a `DeletionOp` with `targetId`, `siteId`, and `clock`.
- Remote replicas mark the character `isDeleted = true`.
- Deleting characters does not remove them from RGA tree traversal, ensuring concurrent insertions after a deleted character converge identically across all replicas.
- Re-applying the same deletion is strictly idempotent.
- Restoring from server reboot preserves all tombstones; deleted characters never ghost back.
- *Status*: **VERIFIED & ACCEPTED** (100% pass in `collabpad_phase6_demo` and `test_deletion.py`).

### AC-2: Asynchronous & Debounced Disk Persistence (Phase 7)
- `DocumentHub::applyOpJson` marks rooms dirty without executing synchronous disk I/O.
- Dirty rooms flush after 1,000ms idle or 3,000ms maximum continuous editing.
- Last client disconnect triggers an immediate vacancy flush.
- Server shutdown (SIGINT / Win32 console close) flushes all dirty rooms.
- High-frequency typing benchmark sustains >200 ops/sec.
- *Status*: **PLANNED / READY FOR EXECUTION** (Plan approved; code implementation pending).

---

## 8. Capability Matrix

| Feature | Design Document / Plan | Actual Implementation | Evidence Level |
|---|---|---|---|
| Lamport Clock Ordering | `core/include/collabpad/LamportClock.hpp` | Implemented | **VERIFIED** |
| RGA Concurrent Insertion | `core/include/collabpad/Sequence.hpp` | Implemented | **VERIFIED** |
| Out-of-Order Causal Buffer | `core/include/collabpad/Replica.hpp` | Implemented | **VERIFIED** |
| Winsock2 WebSocket Server | `server/WebSocketServer.cpp` | Implemented | **VERIFIED** |
| Room Routing & Isolation | `server/DocumentHub.cpp` | Implemented | **VERIFIED** |
| Ephemeral Presence & Cursors | `server/Session.cpp`, `client/` | Implemented | **VERIFIED** |
| Atomic File Snapshots | `server/FileRepository.cpp` | Implemented | **VERIFIED** |
| Dual-Mode Wasm/JS Bridge | `client/src/engine/engineBridge.js` | Implemented | **VERIFIED** |
| RGA Deletions & Tombstones | `core/`, `server/`, `client/` | Implemented | **VERIFIED** |
| Debounced Event-Loop Flusher | Phase 7 Plan | Declared in `DocumentHub.hpp`; pending in `.cpp` | **PLANNED** |
| >200 ops/sec Throughput Test | Phase 7 Plan | Not yet implemented | **PLANNED** |
| Batch / Paste Optimization | Phase 8 Roadmap | Not started | **PLANNED** |
| Multi-Byte UTF-8 Support | Phase 9 Roadmap | Not started | **PLANNED** |
| WSAEventSelect / Token Auth | Phase 10 Roadmap | Not started | **PLANNED** |

---

## 9. Open Questions & Unresolved Decisions

1. **Debounce Interval Fine-Tuning**: Is a 1,000ms idle delay optimal for multi-user high-frequency typing, or should active rooms with multiple typing collaborators shorten debounce to 500ms?  
   *Current Decision*: Maintain 1,000ms idle / 3,000ms max interval as specified in Phase 7 research.
2. **Monotonic Timing Source**: On Windows, `GetTickCount64()` provides millisecond precision. If ported to POSIX platforms in the future, should `std::chrono::steady_clock` be preferred?  
   *Current Decision*: Use `std::chrono::steady_clock` or Win32 `GetTickCount64()` wrapped in a portable helper.
3. **Room Storage Eviction**: In-memory `rooms_` map retains rooms indefinitely during server uptime. When should inactive, clean rooms be evicted from memory?  
   *Resolution*: Deferred to Phase 10 (concurrency and memory hardening).
