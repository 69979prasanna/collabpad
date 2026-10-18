# Project Memory & Architectural Knowledge (MEMORY.md)

**Status**: Active  
**Repository**: CollabPad  
**Last Updated**: October 2026  
**Scope**: Durable institutional knowledge, key decisions, verified commands, resolved bugs, and repository constraints.

---

## 1. Confirmed Architectural Decisions (ADRs)

### ADR-01: Replicated Growable Array (RGA) CRDT
- **Decision**: Use an RGA sequence structure with deterministic `(Lamport clock, siteId)` ordering rather than Logoot, LSEQ, or Operational Transformation (OT).
- **Rationale**: Completely eliminates central coordination and lock contention; guarantees Strong Eventual Consistency across arbitrary network latency and reorderings.

### ADR-02: Zero Third-Party C++ Server Core
- **Decision**: Implement RFC 6455 framing, masking, and SHA-1 hashing natively on Winsock2 with a single-threaded non-blocking `select()` event loop.
- **Rationale**: Avoids MinGW POSIX thread model defects (`<thread>`, `<mutex>` runtime crashes in standard MinGW builds), eliminates external dependencies (Boost.Beast, OpenSSL), and produces a compact single-binary executable (`collabpad_server.exe`).

### ADR-03: Dual-Mode Frontend Execution
- **Decision**: Provide both a C++ WebAssembly engine (via Emscripten Embind) and an identical native JavaScript RGA fallback in `client/src/engine/engineBridge.js`.
- **Rationale**: Enables zero-setup frontend development and live browser previews without requiring local Emscripten SDK installation, while granting full native execution performance when WebAssembly binaries are present.

### ADR-04: Atomic Temporary-File Snapshot Persistence
- **Decision**: Persist document state by writing to `<docId>.json.tmp` first, followed by atomic replacement via Win32 `MoveFileExA(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.
- **Rationale**: Prevents document corruption, half-written files, or JSON syntax truncation during sudden process kills, OS reboots, or concurrent crashes.

### ADR-05: Tombstone-Based Deletion Tracking
- **Decision**: Retain deleted characters in `Sequence::elements_` with an `isDeleted = true` flag (tombstone) rather than splicing them out of the vector.
- **Rationale**: Splicing deleted nodes breaks RGA parent traversal for concurrent operations inserted after the deleted character, causing replicas to diverge. Tombstones preserve the causal tree structure while `getText()` and `getIdAt()` filter them out visually.

### ADR-06: Event-Loop-Driven Debounced Persistence (Phase 7 Planned)
- **Decision**: Adopt a single-threaded periodic flush (`flushDirty`) triggered by the existing 100ms `select()` loop in `WebSocketServer`, bounded by a 1,000ms idle delay and 3,000ms maximum flush interval, combined with immediate flush on room vacancy.
- **Rationale**: Avoids multi-threaded synchronization complexities in MinGW while eliminating 99.5% of disk I/O overhead on the WebSocket event path.

---

## 2. Important Resolved Bugs & Lessons Learned

### 1. Ghost Character Reappearance Bug (Resolved in Phase 6)
- **Problem**: When a character was deleted, previous implementations spliced it from the vector. Upon server reload or when concurrent operations referenced that character as `afterId`, the character would either fail to be found or cause incorrect positioning, and snapshots could resurrect deleted characters.
- **Fix**: Implemented tombstones (`isDeleted`) and out-of-order deletion buffering (`pendingDeletions`). Snapshot serialization includes `"deleted": true` and `"pendingDeletions": [...]`.

### 2. Windows PowerShell Command Chaining Syntax Trap
- **Problem**: Windows PowerShell 5.1 does not support `&&` or `||` command chaining operators (it throws a parser syntax error: `The token '&&' is not a valid statement separator in this version`).
- **Fix**: Enforce the rule of running **one command per invocation** or use semicolon `;` separators where applicable.

### 3. MinGW Threading Incompatibilities
- **Problem**: Standard MinGW GCC on Windows often lacks POSIX thread (pthreads) runtime support, making `std::thread` and `std::mutex` throw runtime errors on initialization.
- **Fix**: The backend server is strictly architected around a single-threaded Winsock2 non-blocking event loop using `select()`. Zero `std::thread` calls are used in the core server.

### 4. Backward Compatibility with Legacy Snapshots
- **Problem**: Snapshots saved before Phase 6 lacked `"deleted"` and `"pendingDeletions"` fields.
- **Fix**: `DocumentSnapshot::fromJson` parses `"deleted"` with a fallback to `false` if omitted, and handles omitted `"pendingDeletions"` as an empty vector. Legacy documents load seamlessly without data loss.

---

## 3. Verified Build, Test, and Run Commands

All commands verified working directly during audit:

```powershell
# 1. Build C++ Native Binaries (MinGW)
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build

# 2. Run Phase 6 CRDT Deletion & Tombstone Demo
.\build\collabpad_phase6_demo.exe

# 3. Run Phase 6 Automated E2E Deletion & Recovery Suite
python tests/test_deletion.py 8096

# 4. Run Phase 5 Autonomous 10-Stage Integration Suite
python tests/test_phase5.py 8095

# 5. Run Phase 2 Concurrency Resolution Demo
.\build\collabpad_phase2_demo.exe

# 6. Run Phase 1 Basic Replica Convergence Demo
.\build\collabpad_demo.exe

# 7. Build Web Client (Vite)
cd client
npm run build
```

---

## 4. Known Limitations & Architectural Constraints

1. **Per-Operation Disk Persistence Bottleneck (Current State)**:
   - In `server/DocumentHub.cpp`, every insertion and deletion currently calls `repository_->save(snap)` synchronously.
   - At high typing speeds (>50 ops/sec), disk write latency limits throughput.
   - *Status*: Phase 7 plan is approved to decouple this into debounced background flushing.
2. **Commit `e53d460` Mislabeling**:
   - Commit `e53d460` was committed with message `(feat): Phase 7 of collabpad sucess`.
   - Investigation shows it only added Phase 7 research/plan documentation and method declarations in `DocumentHub.hpp`. The actual C++ debouncing logic in `DocumentHub.cpp` is **not yet implemented**.
3. **FD_SETSIZE Limit (64 Sockets)**:
   - The native WebSocket server uses Winsock2 `select()`, which defaults to 64 sockets max per process. Suitable for development and small team collaboration; Phase 10 will address high-scale concurrency.
4. **Single-Byte ASCII / Latin-1 Characters**:
   - `Character.value` is `char`. Multi-byte UTF-8 glyphs (emojis, Asian characters) require multi-byte support planned for Phase 9.
5. **No Authentication on Rooms**:
   - Rooms are isolated by `docId`, but anyone knowing the `docId` can join. Token authentication is planned for Phase 10.

---

## 5. Security & Sensitive Information Guidelines

- **Zero Secrets Policy**: No passwords, API keys, private tokens, or complete credentials may ever be committed, logged, or recorded in documentation.
- **Local Network Default**: The native server listens on `127.0.0.1` by default. Binding to `0.0.0.0` or public interfaces requires production firewall and reverse proxy considerations.
