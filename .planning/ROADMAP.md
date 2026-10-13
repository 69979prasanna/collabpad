# CollabPad Project Roadmap

## Milestone 1: Core Foundation & Real-Time Collaboration (v1.0 Baseline)
**Status**: `COMPLETED` (100% Verified)

### Phase 1: Sequential CRDT Engine & Lamport Clock
- **Goal**: Implement baseline character indexing and Lamport timestamp ordering across replicas.
- **Deliverables**:
  - `core/include/collabpad/LamportClock.hpp`
  - `core/include/collabpad/CharId.hpp`
  - `core/include/collabpad/Replica.hpp` (Phase 1 index API)
  - `core/tests/phase1_demo.cpp`
- **Verification**: `collabpad_demo.exe` passes; verified deterministic sequential convergence.

### Phase 2: RGA Concurrent Engine & Dependency Buffer
- **Goal**: Eliminate positional race conditions using Replicated Growable Array (RGA) ordering and causal dependency buffering.
- **Deliverables**:
  - `core/include/collabpad/Operation.hpp` (`InsertionOp`)
  - `core/include/collabpad/Sequence.hpp` (Tree traversal and `findInsertIndex`)
  - `core/include/collabpad/Replica.hpp` (RGA API + `pendingOperations_` buffer)
  - `core/tests/phase2_demo.cpp`
- **Verification**: `collabpad_phase2_demo.exe` passes; verified concurrent insertions after identical parents and buffering of out-of-order deliveries.

### Phase 3: Native Winsock2 WebSocket Server & Room Relay
- **Goal**: Implement zero-dependency native C++ WebSocket server using Winsock2 with RFC 6455 framing and room isolation.
- **Deliverables**:
  - `server/WebSocketServer.hpp/.cpp` (`select()` event loop)
  - `server/Sha1.hpp` (Native SHA-1 & Base64 handshake digest)
  - `server/Session.hpp/.cpp` (Frame parsing, masking/unmasking, JSON dispatch)
  - `server/DocumentHub.hpp/.cpp` (Room isolation and non-echo broadcasting)
- **Verification**: `tests/test_server.py` passes; confirmed RFC 6455 handshake, message routing, non-echo broadcasting, and room boundaries.

### Phase 4: Collaborative React Web Application & Live Presence
- **Goal**: Deliver modern dark engineering interface with real-time editor, collaborator cursors, presence, and room switching.
- **Deliverables**:
  - `client/src/App.jsx`, `client/src/index.css`
  - `client/src/components/Editor.jsx` (Gutter line numbering, monospace surface, remote cursors)
  - `client/src/components/Toolbar.jsx`, `Sidebar.jsx`, `CollaboratorsPanel.jsx`
  - `client/src/hooks/useCollabSocket.js` (Throttled cursors, presence heartbeats, bounded backoff)
- **Verification**: `tests/test_collab_e2e.py` passes; multi-client typing, cursor position streaming, and presence validated.

### Phase 5: Durable Persistence, Crash Recovery & WebAssembly Bridge
- **Goal**: Ensure durable document survival across server reboots, implement missed-operation catch-up, and provide WebAssembly acceleration.
- **Deliverables**:
  - `core/include/collabpad/DocumentSnapshot.hpp` (JSON snapshot serialization)
  - `server/DocumentRepository.hpp` & `server/FileRepository.cpp` (Atomic `.tmp` -> `.json` writes via `MoveFileExA`)
  - `wasm/bindings.cpp` & `wasm/build_wasm.ps1` (Emscripten Embind exports)
  - `client/src/engine/engineBridge.js` (Dual-mode Wasm + JS fallback engine)
  - `tests/test_phase5.py`
- **Verification**: `tests/test_phase5.py` (10-stage autonomous suite) passes with 100% success across restarts and disconnects.

---

## Milestone 2: Production Hardening & Advanced CRDT (v1.1 Planned)
**Status**: `PLANNED` (Next Active Milestone)

### Phase 6: RGA Deletion & Tombstone Tracking
- **Goal**: Enable cooperative character deletion (backspace/delete) preserving causal order across replicas.
- **Deliverables**:
  - Add `bool isDeleted` flag to `collabpad::Character`.
  - Add `DeletionOp` with target `CharId`.
  - Update `Sequence::getText()` to filter tombstones while maintaining them in causal tree traversal.
  - Implement deletion support in `client/src/engine/engineBridge.js` (both JS and Wasm).
  - Add automated test verifying concurrent delete-insert and delete-delete idempotency.

### Phase 7: Asynchronous & Debounced Disk Persistence
- **Goal**: Eliminate synchronous disk rewrites on every keystroke in `DocumentHub`.
- **Deliverables**:
  - Implement dirty tracking flusher: persist snapshots periodically (e.g. 2s interval) or upon room vacancy.
  - Optional append-only operation log (WAL) for microsecond write confirmation.
  - Add stress test verifying high-frequency typing throughput (>200 ops/sec).

### Phase 8: Batch Operations & Paste Optimization
- **Goal**: Prevent network flooding when users paste multi-line text blocks.
- **Deliverables**:
  - Add `BatchInsertionOp` / string span insertion to wire protocol.
  - Optimize `Editor.jsx` text diffing to generate single batch operation on paste.
  - Client and server support for atomic multi-character insertion.

### Phase 9: Multi-Byte UTF-8 & Unicode Support
- **Goal**: Protect multi-byte UTF-8 characters (emojis, accented characters, CJK scripts) against concurrent byte interleaving.
- **Deliverables**:
  - Migrate `Character.value` from single `char` to `uint32_t` (Unicode code point) or UTF-8 byte span.
  - Update `DocumentSnapshot` JSON escaping to support full UTF-8.
  - Add automated test verifying concurrent typing of multi-byte glyphs and emojis.

### Phase 10: High-Concurrency Event Loop & Security Hardening
- **Goal**: Scale beyond 64 sockets and add access control.
- **Deliverables**:
  - Increase `FD_SETSIZE` or migrate to `WSAEventSelect` / IOCP on Windows.
  - Token-based room access control and handshake validation.
  - Production TLS/WSS reverse proxy configuration guide.
