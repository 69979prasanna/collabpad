# Phase 7 Plan: Asynchronous & Debounced Disk Persistence

## Phase Overview
- **Phase**: 07
- **Name**: `asynchronous-debounced-persistence`
- **Goal**: Remove synchronous disk persistence from the per-operation WebSocket event path in `DocumentHub`, replacing it with an event-loop-driven debounced flusher (with idle timeout and maximum flush interval), immediate room-vacancy flushing, and crash-safe shutdown, while targeting >200 ops/sec sustained throughput.
- **Estimated Scope**: 3 Waves / 8 Detailed Tasks
- **Prerequisites**: Phases 1–6 completed and verified.
- **Backwards Compatibility**: 100% preservation of all existing Phase 1–6 wire protocols, snapshot JSON formats, tombstones, and test suites.

---

## Task Decomposition

### Wave 1: Core Debounce & Timing Logic in `DocumentHub`

#### Task 1.1: Timing Metadata & Configuration in `DocumentRoom`
- **Target File**: `server/DocumentHub.hpp`
- **Changes**:
  1. Add timing and debounce fields to `struct DocumentRoom`:
     ```cpp
     uint64_t firstDirtyTimeMs{0};  // When the room transitioned from clean to dirty
     uint64_t lastEditTimeMs{0};    // Timestamp of the most recent operation
     ```
  2. Add debounce configuration constants or parameters to `DocumentHub`:
     ```cpp
     static constexpr uint64_t DEFAULT_DEBOUNCE_DELAY_MS = 1000;    // 1.0s idle
     static constexpr uint64_t DEFAULT_MAX_FLUSH_INTERVAL_MS = 3000; // 3.0s max
     ```
  3. Add public member functions:
     ```cpp
     // Evaluates dirty rooms against timing thresholds and persists eligible rooms.
     // Returns number of rooms flushed.
     size_t flushDirty(uint64_t nowMs,
                       uint64_t debounceDelayMs = DEFAULT_DEBOUNCE_DELAY_MS,
                       uint64_t maxFlushIntervalMs = DEFAULT_MAX_FLUSH_INTERVAL_MS);

     // Check if a specific room is dirty
     bool isRoomDirty(const std::string& docId) const;
     ```
- **Verification**: `DocumentHub.hpp` compiles cleanly.

#### Task 1.2: Decouple Per-Operation Persistence in `applyOpJson`
- **Target File**: `server/DocumentHub.cpp`
- **Changes**:
  1. In `DocumentHub::applyOpJson(DocumentRoom& room, const string& json)`:
     - When an insertion operation is applied:
       - Update `room.isDirty = true`.
       - Record timestamps: if `room.firstDirtyTimeMs == 0`, set `room.firstDirtyTimeMs = nowMs`. Set `room.lastEditTimeMs = nowMs`.
       - **Remove** synchronous calls:
         ```cpp
         // REMOVE synchronous snapshot creation and save:
         // DocumentSnapshot snap = room.replica->createSnapshot(room.docId, room.title);
         // repository_->save(snap);
         // room.isDirty = false;
         ```
     - When a deletion operation is applied:
       - Update `room.isDirty = true`.
       - Record timestamps: if `room.firstDirtyTimeMs == 0`, set `room.firstDirtyTimeMs = nowMs`. Set `room.lastEditTimeMs = nowMs`.
       - **Remove** synchronous `repository_->save(...)` call.
- **Verification**: Operations mutate in-memory replica immediately without executing synchronous disk I/O.

#### Task 1.3: Debounced Flush Evaluation (`flushDirty`)
- **Target File**: `server/DocumentHub.cpp`
- **Changes**:
  1. Implement `size_t DocumentHub::flushDirty(uint64_t nowMs, uint64_t debounceDelayMs, uint64_t maxFlushIntervalMs)`:
     - Iterate through `rooms_`.
     - For each room where `room.isDirty == true`:
       - Check if `(nowMs - room.lastEditTimeMs >= debounceDelayMs)` OR `(nowMs - room.firstDirtyTimeMs >= maxFlushIntervalMs)`:
         - Attempt `saveDocument(docId)`.
         - If save succeeds:
           - `room.isDirty = false`.
           - `room.firstDirtyTimeMs = 0`.
           - `room.lastEditTimeMs = 0`.
         - If save fails:
           - Log error and keep `room.isDirty = true` and `firstDirtyTimeMs` intact so it retries on subsequent ticks.
  2. Update `DocumentHub::saveDocument(const string& docId)`:
     - Ensure successful save resets `firstDirtyTimeMs = 0` and `lastEditTimeMs = 0`.
- **Verification**: Unit/standalone test verifies debounce timer flushes after 1s idle and forces flush after 3s continuous typing.

---

### Wave 2: Event Loop Integration, Room Vacancy & Shutdown

#### Task 2.1: Periodic Tick Hook in `WebSocketServer`
- **Target File**: `server/WebSocketServer.cpp`
- **Changes**:
  1. In `WebSocketServer::run()`:
     - Retrieve current monotonic time in milliseconds (e.g. using `GetTickCount64()` on Windows or `std::chrono::steady_clock`).
     - At the end of each `select()` loop iteration (after socket reads and `pruneDeadSessions()`):
       ```cpp
       uint64_t now = getCurrentTimeMs();
       hub_->flushDirty(now);
       ```
     - Since `select()` has a 100ms timeout, the event loop ticks at least once every 100ms (and immediately after any socket activity).
- **Verification**: Server automatically flushes dirty rooms in the background during active typing pauses.

#### Task 2.2: Immediate Room-Vacancy Flush
- **Target File**: `server/DocumentHub.cpp`
- **Changes**:
  1. In `DocumentHub::leave(const string& docId, uint32_t sessionId)`:
     - After erasing the session from `room.sessions`:
     - If `room.sessions.empty()` AND `room.isDirty`:
       ```cpp
       if (room.sessions.empty() && room.isDirty && repository_) {
           cout << "[Hub] Last participant left room \"" << docId << "\". Executing immediate vacancy flush.\n";
           saveDocument(docId);
       }
       ```
- **Verification**: When the last user disconnects, the document is immediately flushed to disk regardless of remaining debounce time.

#### Task 2.3: Windows Console & Signal Shutdown Hardening
- **Target File**: `server/main.cpp`
- **Changes**:
  1. Register Win32 console control handler:
     ```cpp
     #ifdef _WIN32
     static BOOL WINAPI consoleCtrlHandler(DWORD ctrlType) {
         if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_CLOSE_EVENT ||
             ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_SHUTDOWN_EVENT) {
             cout << "\n[Main] Shutdown event received. Flushing dirty documents...\n";
             if (g_hub) g_hub->flushAll();
             if (g_server) g_server->stop();
             return TRUE;
         }
         return FALSE;
     }
     #endif
     ```
  2. In `main()`:
     - Call `SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);`.
     - Retain `signal(SIGINT, onSignal);` for portable fallback.
     - Retain `hub->flushAll();` after `server.run()` finishes.
- **Verification**: Clean server shutdown via Ctrl+C, window close, or process termination flushes all dirty documents to disk.

---

### Wave 3: Automated Verification, Benchmarking & Regression Suite

#### Task 3.1: Automated Debounce & Persistence Test Suite
- **Target File**: `tests/test_debounced_persistence.py` (New)
- **Verification Cases**:
  1. **Idle Debounce**: Send 5 keystrokes within 200ms -> verify NO disk write occurs immediately -> wait 1.2s -> verify single disk write occurred.
  2. **Max Flush Interval**: Continuously send 1 keystroke every 100ms for 3.5s -> verify disk write occurs at ~3.0s despite continuous editing.
  3. **Room Vacancy Flush**: Send keystrokes, disconnect client immediately -> verify file on disk is immediately updated without waiting for 1–3s timers.
  4. **Save Failure & Dirty State Retention**: Simulate temporary write failure -> verify `isDirty` remains true and retries successfully.
  5. **Tombstone & Deletion Preservation**: Send delete operations -> verify tombstones and pending deletes are durably written to disk during debounced save.

#### Task 3.2: High-Throughput Stress Benchmark (>200 ops/sec)
- **Target File**: `tests/test_persistence_throughput.py` (New)
- **Verification Cases**:
  1. Sustained throughput measurement: Send 500 character operations in rapid succession over WebSocket.
  2. Verify sustained throughput exceeds **200 ops/sec** (target > 500 ops/sec).
  3. Verify disk write count during burst is $\le 3$ (proving debouncing efficacy).
  4. Verify all 500 characters converge accurately on disk and survive server restart.

#### Task 3.3: Full Regression Suite Verification
- **Target Files**:
  - `collabpad_phase6_demo.exe`
  - `collabpad_phase2_demo.exe`
  - `collabpad_demo.exe`
  - `tests/test_deletion.py`
  - `tests/test_phase5.py`
  - `tests/test_collab_e2e.py`
- **Verification**: 100% pass across all existing test suites.

---

## Acceptance Criteria & Regression Gates

- [ ] `DocumentHub::applyOpJson` performs 0 synchronous disk writes during insertion/deletion.
- [ ] Dirty rooms are persisted when idle for `debounceDelayMs` (1.0s).
- [ ] Dirty rooms are guaranteed to be persisted after `maxFlushIntervalMs` (3.0s) during continuous typing.
- [ ] Room vacancy (last client disconnect) triggers an immediate snapshot save.
- [ ] Graceful server shutdown (Ctrl+C, console close, SIGINT) commits all pending dirty rooms.
- [ ] Save failures preserve `isDirty = true` and retry on the next tick without crashing or dropping WebSocket sessions.
- [ ] Phase 6 deletion tombstones (`isDeleted`) and `pendingDeletions` are preserved across debounced snapshots.
- [ ] `tests/test_persistence_throughput.py` passes with >200 ops/sec sustained throughput and zero dropped frames.
- [ ] All 10 existing tests in `test_phase5.py` continue to pass.
- [ ] All 7 existing tests in `test_deletion.py` continue to pass.
- [ ] All C++ demos (`collabpad_phase6_demo.exe`, `collabpad_phase2_demo.exe`, `collabpad_demo.exe`) exit with code 0.

---

## Risk Analysis & Mitigations

| Risk | Impact | Mitigation Strategy |
|---|---|---|
| Event loop stall during periodic flush | Jitter on high document sizes (>1MB) | `FileRepository::save()` takes <2ms for documents up to 5,000 characters. For large documents, debouncing limits writes to at most once per 3 seconds. |
| Server crash between keystroke and flush | Potential loss of last 1–3s of edits | Mitigated by bounding max flush interval to 3s and immediate flush on vacancy. Web clients retain local replica state and reconnect automatically. |
| Time wrap-around / non-monotonic clock | Timer calculation anomalies | Use `GetTickCount64()` on Windows (monotonic 64-bit integer, never wraps around during process lifecycle). |
| Out-of-order delete arriving before flush | Missing tombstone on disk | `DocumentSnapshot` includes `pendingDeletions` array, capturing un-applied deletes durably in every flush. |
