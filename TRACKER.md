# Project Status & Task Tracker (TRACKER.md)

**Status**: Active  
**Milestone**: Milestone 2 (Production Hardening & Advanced CRDT)  
**Current Phase**: Phase 7 (Asynchronous & Debounced Disk Persistence)  
**Current State**: READY FOR EXECUTION (Phase 6 Completed & Verified; Phase 7 Planned & Bounded)  
**Last Audit**: October 2026

---

## 1. Verified Project State Summary

| Phase | Description | Status | Evidence Level | Verification Evidence |
|---|---|---|---|---|
| **Phase 1** | Sequential CRDT Engine & Lamport Clock | **COMPLETED** | **VERIFIED** | `collabpad_demo.exe` passes; sequential convergence confirmed. |
| **Phase 2** | RGA Concurrency Engine & Dependency Buffer | **COMPLETED** | **VERIFIED** | `collabpad_phase2_demo.exe` passes; concurrent sibling ordering and causal buffering confirmed. |
| **Phase 3** | Winsock2 Native WebSocket Server & Room Relay | **COMPLETED** | **VERIFIED** | `tests/test_server.py` passes; RFC 6455 handshake, frame unmasking, non-echo broadcast confirmed. |
| **Phase 4** | React Web Application & Live Presence | **COMPLETED** | **VERIFIED** | `client/` builds via `npm run build`; `tests/test_collab_e2e.py` passes multi-client cursors and presence. |
| **Phase 5** | Durable Persistence, Crash Recovery & WASM Bridge | **COMPLETED** | **VERIFIED** | `tests/test_phase5.py` passes 10/10 tests including server restart and crash recovery. |
| **Phase 6** | RGA Deletion & Tombstone Tracking | **COMPLETED** | **VERIFIED** | `collabpad_phase6_demo.exe` (6/6 passed) and `tests/test_deletion.py` (7/7 passed); zero ghost characters. |
| **Phase 7** | Asynchronous & Debounced Disk Persistence | **IN-PLANNING / READY** | **PLANNED** | Approved plan in `.planning/phases/07-asynchronous-debounced-persistence/PLAN.md`. Declarations in `DocumentHub.hpp` verified; cpp implementation pending. |
| **Phase 8** | Batch Operations & Paste Optimization | **NOT STARTED** | **PLANNED** | Scheduled in Roadmap for Milestone 2. |
| **Phase 9** | Multi-Byte UTF-8 & Unicode Support | **NOT STARTED** | **PLANNED** | Scheduled in Roadmap for Milestone 2. |
| **Phase 10**| High-Concurrency Event Loop & Security | **NOT STARTED** | **PLANNED** | Scheduled in Roadmap for Milestone 2. |

> [!IMPORTANT]
> **Audit Reconciliation Note**: Git commit `e53d460` was titled `(feat): Phase 7 of collabpad sucess`. Audit confirms that this commit only introduced Phase 7 research/plan documents and added method declarations to `DocumentHub.hpp`. The actual debounced persistence logic in `DocumentHub.cpp`, `WebSocketServer.cpp`, and benchmark tests has **NOT** been implemented yet. Phase 7 is officially **READY FOR EXECUTION**, not completed.

---

## 2. Phase 7 Task Breakdown & Implementation Status

Phase Plan: [.planning/phases/07-asynchronous-debounced-persistence/PLAN.md](file:///.planning/phases/07-asynchronous-debounced-persistence/PLAN.md)

### Wave 1: Core Debounce & Timing Logic in `DocumentHub`
- [x] **Task 1.1**: Timing Metadata & Configuration in `DocumentRoom` (`server/DocumentHub.hpp`)  
  *Status*: **VERIFIED (Declarations Added)** — `firstDirtyTimeMs`, `lastEditTimeMs`, `flushDirty`, and `isRoomDirty` declared.
- [ ] **Task 1.2**: Decouple Per-Operation Persistence in `applyOpJson` (`server/DocumentHub.cpp`)  
  *Status*: **PLANNED / NEXT AUTHORIZED TASK** — Remove per-keystroke `repository_->save()` calls, mark room dirty, record monotonic edit timestamps.
- [ ] **Task 1.3**: Implement Debounced Flush Evaluation `flushDirty` (`server/DocumentHub.cpp`)  
  *Status*: **PLANNED** — Implement 1.0s idle delay and 3.0s maximum flush interval flusher.

### Wave 2: Event Loop Integration, Room Vacancy & Shutdown
- [ ] **Task 2.1**: Periodic Tick Hook in `WebSocketServer.cpp`  
  *Status*: **PLANNED** — Hook `hub_->flushDirty(nowMs)` into `select()` event loop.
- [ ] **Task 2.2**: Immediate Room-Vacancy Flush in `DocumentHub.cpp`  
  *Status*: **PLANNED** — Trigger immediate save when the last active session leaves a dirty room.
- [ ] **Task 2.3**: Windows Console & Signal Shutdown Hardening in `server/main.cpp`  
  *Status*: **PLANNED** — Register `SetConsoleCtrlHandler` and call `hub_->flushAll()` on exit.

### Wave 3: Automated Verification, Benchmarking & Regression
- [ ] **Task 3.1**: Automated Debounce & Persistence Test Suite (`tests/test_debounced_persistence.py`)  
  *Status*: **PLANNED** — Test idle debounce (1s), max flush interval (3s), vacancy flush, and retry on save failure.
- [ ] **Task 3.2**: High-Throughput Stress Benchmark (`tests/test_persistence_throughput.py`)  
  *Status*: **PLANNED** — Prove >200 ops/sec sustained throughput with burst writes $\le 3$ disk writes.
- [ ] **Task 3.3**: Full Regression Suite Verification  
  *Status*: **PLANNED** — Verify 0 regressions across `test_deletion.py`, `test_phase5.py`, and C++ demos.

---

## 3. Single Authorized Next Task

### **Task: Phase 7 Wave 1 Task 1.2 — Decouple Per-Operation Persistence in `applyOpJson`**
- **Objective**: In `server/DocumentHub.cpp`, modify `applyOpJson` so that applied insertion and deletion operations do NOT perform synchronous `repository_->save(...)` calls. Instead, set `room.isDirty = true` and update `room.firstDirtyTimeMs` and `room.lastEditTimeMs`.
- **Target File**: `server/DocumentHub.cpp` (and verify header compatibility in `server/DocumentHub.hpp`).
- **Prerequisites**: Task 1.1 declarations confirmed present.
- **Required Tests & Completion Criteria**:
  1. Build succeeds: `cmake --build build` (0 compiler errors).
  2. In-memory replica continues to apply operations correctly.
  3. No synchronous disk writes are executed on the WebSocket relay thread during `applyOpJson`.
  4. Working tree remains clean; no modifications to unrelated files.

---

## 4. Known Blockers & Open Questions

1. **Monotonic Clock Implementation**: Ensure the millisecond timing helper used for `nowMs` is monotonic and doesn't wrap around (recommendation: `std::chrono::steady_clock` or Win32 `GetTickCount64()`).
2. **Server Shutdown Handler Portability**: Windows console close events require `SetConsoleCtrlHandler`. Ensure it is enclosed within `#ifdef _WIN32` so native code remains portable.

---

## 5. Short Project Change Log

- **Commit `c133fc4`**: Implemented CollabPad Phase 6 deletion support (tombstones, `DeletionOp`, `pendingDeletions`, snapshot serialization).
- **Commit `92451c9`**: Fixed deletion bug in Phase 6 (verified 7/7 automated tests passing in `tests/test_deletion.py`).
- **Commit `e53d460`**: Added Phase 7 research and plan documents; declared `flushDirty` and timing fields in `server/DocumentHub.hpp`.
- **Current Milestone Session**: Repository audit and project control system initialization (`PRD.md`, `DESIGN.md`, `MEMORY.md`, `RULES.md`, `TRACKER.md`). All tests confirmed green.
