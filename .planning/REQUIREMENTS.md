# Requirements & Traceability Matrix

## Status Legend
- `[x]` **Completed**: Built, verified, and passing tests.
- `[ ]` **Planned**: Scheduled for upcoming milestone phases.
- `[?]` **Backlog / Proposed**: Architectural considerations for future releases.

---

## 1. Core CRDT Engine (RGA)

| ID | Requirement | Status | Verification & Target |
|---|---|---|---|
| **CRDT-01** | Lamport logical clock synchronization on each local and remote operation. | `[x]` | `core/tests/phase1_demo.cpp` |
| **CRDT-02** | Globally unique `CharId(siteId, clock)` assigned to every character. | `[x]` | `core/include/collabpad/CharId.hpp` |
| **CRDT-03** | Deterministic RGA sibling ordering for concurrent insertions after identical parents. | `[x]` | `core/tests/phase2_demo.cpp` |
| **CRDT-04** | Causal dependency buffer holding out-of-order operations until parent arrives. | `[x]` | `core/tests/phase2_demo.cpp` |
| **CRDT-05** | JSON serialization and deserialization of complete document snapshots. | `[x]` | `tests/test_phase5.py` |
| **CRDT-06** | Operation idempotency preventing duplicate node generation on repeated deliveries. | `[x]` | `tests/test_phase5.py` |
| **CRDT-07** | Deletion operations using tombstones (`isDeleted`) preserving causal ordering. | `[ ]` | Milestone 2 (Phase 6) |
| **CRDT-08** | Multi-byte UTF-8 character / Unicode code point support. | `[ ]` | Milestone 2 (Phase 9) |

---

## 2. Server & WebSocket Relay

| ID | Requirement | Status | Verification & Target |
|---|---|---|---|
| **SRV-01** | Custom RFC 6455 handshake, SHA-1 key hashing, and Base64 response encoding. | `[x]` | `tests/test_server.py` |
| **SRV-02** | RFC 6455 binary frame decoding, mask validation, and unmasking. | `[x]` | `tests/test_server.py` |
| **SRV-03** | Single-threaded `select()` non-blocking I/O multiplexing via Winsock2. | `[x]` | `build/collabpad_server.exe` |
| **SRV-04** | Document room multi-tenancy and strict isolation between rooms. | `[x]` | `tests/test_server.py`, `tests/test_phase5.py` |
| **SRV-05** | Non-echo message fanout (broadcasting to peers excluding sender). | `[x]` | `tests/test_server.py` |
| **SRV-06** | Session lifecycle tracking with instant leave/offline broadcast on disconnect. | `[x]` | `tests/test_phase5.py` |
| **SRV-07** | High-concurrency event model overcoming default 64 `FD_SETSIZE` socket limit. | `[ ]` | Milestone 2 (Phase 10) |
| **SRV-08** | Authentication / token-based room authorization. | `[ ]` | Milestone 2 (Phase 10) |

---

## 3. Persistence & Storage

| ID | Requirement | Status | Verification & Target |
|---|---|---|---|
| **PST-01** | Decoupled `DocumentRepository` abstract interface. | `[x]` | `server/DocumentRepository.hpp` |
| **PST-02** | File-backed repository (`FileRepository`) writing persistent JSON snapshots. | `[x]` | `server/FileRepository.cpp` |
| **PST-03** | Atomic temporary-file replacement (`.tmp` -> `.json` via `MoveFileExA`). | `[x]` | `tests/test_phase5.py` |
| **PST-04** | Automatic document recovery across server termination and restart. | `[x]` | `tests/test_phase5.py` |
| **PST-05** | Asynchronous / debounced disk persistence or append-only write log (WAL). | `[ ]` | Milestone 2 (Phase 7) |

---

## 4. Web Client & User Experience

| ID | Requirement | Status | Verification & Target |
|---|---|---|---|
| **CLI-01** | Modern dark engineering UI with custom design tokens. | `[x]` | `client/src/index.css` |
| **CLI-02** | Monospace editor surface with dynamic line numbering and scroll lock. | `[x]` | `client/src/components/Editor.jsx` |
| **CLI-03** | Multi-document workspace switching and direct room joining. | `[x]` | `client/src/components/Sidebar.jsx` |
| **CLI-04** | Real-time collaborator cursor tracking with name badges and unique colors. | `[x]` | `client/src/components/Editor.jsx` |
| **CLI-05** | Presence indicators (Active / Typing / Idle) and activity feed stream. | `[x]` | `client/src/components/CollaboratorsPanel.jsx` |
| **CLI-06** | Throttled cursor transmission (45ms–50ms) to prevent network saturation. | `[x]` | `client/src/hooks/useCollabSocket.js` |
| **CLI-07** | Bounded exponential backoff reconnection with jitter (1s–15s). | `[x]` | `client/src/hooks/useCollabSocket.js` |
| **CLI-08** | Offline operation buffer flushed automatically on connection re-establishment. | `[x]` | `client/src/hooks/useCollabSocket.js` |
| **CLI-09** | Batch operations for multi-character text paste handling. | `[ ]` | Milestone 2 (Phase 8) |

---

## 5. WebAssembly & Bridge

| ID | Requirement | Status | Verification & Target |
|---|---|---|---|
| **WASM-01** | Emscripten Embind exports for `CharId`, `InsertionOp`, `Sequence`, `Replica`. | `[x]` | `wasm/bindings.cpp` |
| **WASM-02** | Dual-mode frontend bridge automatically detecting WebAssembly availability. | `[x]` | `client/src/engine/engineBridge.js` |
| **WASM-03** | Zero-configuration JavaScript RGA fallback with identical ordering rules. | `[x]` | `client/src/engine/engineBridge.js` |
| **WASM-04** | Visual engine status indicator (WebAssembly vs JavaScript Fallback). | `[x]` | `client/src/components/WasmStatusBanner.jsx` |

---

## Traceability & Coverage Matrix

| Requirement Cluster | Code Locations | Automated Tests |
|---|---|---|
| CRDT Algorithm | `core/include/collabpad/` | `collabpad_demo`, `collabpad_phase2_demo`, `test_phase5.py` |
| Server Networking | `server/WebSocketServer.*`, `server/Session.*` | `test_server.py`, `test_collab_e2e.py` |
| Room Routing & Isolation | `server/DocumentHub.*` | `test_server.py`, `test_phase5.py` |
| Storage & Recovery | `server/FileRepository.*` | `test_phase5.py` |
| Frontend React & Sockets | `client/src/` | Manual verification (`npm run dev`) + `test_collab_e2e.py` |
| WebAssembly Bridge | `wasm/`, `client/src/engine/engineBridge.js` | `test_phase5.py`, Browser Wasm Status Banner |
