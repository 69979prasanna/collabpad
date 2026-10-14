# Phase 6 Plan: RGA Deletion & Tombstone Tracking

## Phase Overview
- **Phase**: 06
- **Name**: `rga-deletion-tombstones`
- **Goal**: Enable cooperative character deletion (backspace/delete) using RGA tombstones, preserving causal ordering across replicas and eliminating the "ghost character reappearance" bug.
- **Estimated Scope**: 4 Waves / 5 Core Tasks
- **Prerequisites**: Phases 1–5 completed and verified.
- **Backwards Compatibility**: 100% preservation of all existing Phase 1–5 interfaces and test suites.

---

## Task Decomposition

### Wave 1: Core CRDT Engine Extensions (C++17)

#### Task 1.1: Tombstone Support in `Sequence.hpp`
- **Target File**: `core/include/collabpad/Sequence.hpp`
- **Changes**:
  1. Add `bool isDeleted{false};` to `struct Character`.
  2. Update constructors of `Character` to initialize `isDeleted(false)`.
  3. Add constructor overload: `Character(CharId charId, CharId parentId, char val, bool del)`.
  4. Update `getText()`: append `ch.value` only if `!ch.isDeleted`.
  5. Add `size_t visibleSize() const`: count elements where `!isDeleted`.
  6. Update `getIdAt(size_t visualIndex) const`: iterate through elements, incrementing visual counter only for `!isDeleted`, returning `id` of the visual match.
  7. Update `findIndex(const CharId& id) const`: return the 0-based visual index among non-deleted characters (or `-1` if tombstoned / not found).
  8. Add `bool markDeleted(const CharId& id)`: find character with `id`, set `isDeleted = true`, return true if found and newly marked.
  9. Add `bool isTombstone(const CharId& id) const`.
  10. **Preserve**: `findInsertIndex()` sibling traversal continues to traverse all elements (including tombstones), ensuring concurrent insertions after a deleted parent succeed deterministically.

#### Task 1.2: Deletion Operation & Replica Integration
- **Target Files**:
  - `core/include/collabpad/Operation.hpp`
  - `core/include/collabpad/Replica.hpp`
- **Changes**:
  1. In `Operation.hpp`: Add `struct DeletionOp { CharId targetId; uint32_t siteId{0}; uint64_t clock{0}; };`.
  2. In `Replica.hpp`:
     - Add `DeletionOp localDelete(const CharId& targetId)`: ticks Lamport clock, calls `sequence_.markDeleted(targetId)`, returns `DeletionOp`.
     - Add `DeletionOp localDeleteAt(size_t visualIndex)`: resolves `targetId = sequence_.getIdAt(visualIndex)`, calls `localDelete(targetId)`.
     - Add `bool applyDelete(const DeletionOp& op)`: updates clock `clock_.update(op.clock)`, applies `sequence_.markDeleted(op.targetId)`.
     - Add `pendingDeletions_` buffer (set of `CharId`): if `applyDelete` receives a `targetId` not yet in `sequence_`, buffer it; when `applyOperation` inserts the target character, immediately apply the tombstone.
     - Add `visibleSize() const` delegating to `sequence_.visibleSize()`.

#### Task 1.3: Document Snapshot Serialization
- **Target File**: `core/include/collabpad/DocumentSnapshot.hpp`
- **Changes**:
  1. In `toJson()`: Add `"deleted":` boolean field for each character object in `"characters"`, and add `"pendingDeletions": [ {"siteId": ..., "clock": ...}, ... ]` array capturing any buffered deletes whose targets have not arrived.
  2. In `fromJson()`: Parse `"deleted"` sub-field if present (defaulting to `false` if omitted for backwards compatibility) and parse `"pendingDeletions"` array (empty if omitted).
  3. In `Replica::loadSnapshot()`: Restore `pendingDeletions_` buffer from the snapshot so that pending deletes survive server crash and restart recovery.

---

### Wave 2: Server Protocol & Storage Relay

#### Task 2.1: Relay & Persistence of Deletions in `DocumentHub`
- **Target File**: `server/DocumentHub.cpp`
- **Changes**:
  1. Update `applyOpJson(DocumentRoom& room, const string& json)`:
     - Detect if `op` is a delete operation (check for `"type": "delete"` or presence of `"targetId"`).
     - If delete: parse the deletion's own `siteId` and `clock` (advancing the server replica's clock via `clock_.update(op.clock)`) and parse `targetId.siteId` and `targetId.clock`. Construct `DeletionOp(targetId, opSite, opClock)` and call `room.replica->applyDelete(op)`.
     - Set `room.isDirty = true` and invoke `repository_->save(...)` to persist updated snapshot with tombstones and durable `pendingDeletions`.
  2. In `broadcast()`:
     - Relay delete operations (carrying `siteId`, `clock`, and `targetId`) to all other connected sessions in the room (non-echo fanout).

---

### Wave 3: Dual-Mode Engine Bridge & Client UI

#### Task 3.1: JavaScript Engine Bridge Deletion Logic
- **Target File**: `client/src/engine/engineBridge.js`
- **Changes**:
  1. Add `isDeleted = false` to JavaScript `Character` class.
  2. In `Sequence`:
     - Implement `markDeleted(id)`
     - Update `getText()` to ignore `isDeleted`
     - Update `getIdAt(visualIndex)` to map 0-based visible index to `CharId`
     - Update `findIndex(id)` to return visible index
     - Add `visibleSize()` / update `size()`
  3. In `EngineBridge`:
     - Implement `localDelete(targetId)`
     - Implement `applyDelete(op)`: advance clock via `updateClock(op.clock)`, apply `markDeleted`, or buffer in `pendingDeletions`
     - Update `applyOperation(rawOp)`: branch on `rawOp.type === 'delete'` to parse `rawOp.siteId`, `rawOp.clock`, and `rawOp.targetId`, advancing clock and invoking `applyDelete`
     - Buffer out-of-order deletes in `pendingDeletions`

#### Task 3.2: Frontend Text Diffing in `App.jsx`
- **Target File**: `client/src/App.jsx`
- **Changes**:
  1. In `handleLocalTextChange(newText, cursorPos)`:
     - Compute common prefix and suffix.
     - Calculate deleted slice: `deletedCount = oldText.length - prefix - suffix`.
     - If `deletedCount > 0`:
       - Iterate through deleted characters from visual index `prefix + deletedCount - 1` down to `prefix`:
         - Retrieve `targetId = engineRef.current.getIdAt(i)`.
         - Execute `delOp = engineRef.current.localDelete(targetId)`.
         - Transmit operation over WebSocket carrying the deletion's own timestamp and target:
           ```javascript
           sendOperation({
             type: 'delete',
             siteId: delOp.siteId,
             clock: delOp.clock,
             targetId: { siteId: delOp.targetId.siteId, clock: delOp.targetId.clock }
           });
           ```
     - Calculate inserted slice: `insertedCount = newText.length - prefix - suffix`.
     - If `insertedCount > 0`: insert characters as currently done.
  2. Update `handleRemoteOp`:
     - Handle incoming delete operations seamlessly.

---

### Wave 4: WebAssembly Bindings & Automated Verification

#### Task 4.1: WebAssembly Embind Exports
- **Target File**: `wasm/bindings.cpp`
- **Changes**:
  1. Export `DeletionOp` value object (`targetId`, `siteId`, `clock`).
  2. Export `localDelete`, `applyDelete`, `markDeleted` on `Replica`.

#### Task 4.2: C++ Verification Demo Target
- **Target Files**:
  - `core/tests/phase6_demo.cpp` (New)
  - `CMakeLists.txt`
- **Verification Cases**:
  1. Sequential insert and delete (visible text correctly omits deleted characters).
  2. Concurrent edit resolution: Replica A deletes `'A'`, Replica B inserts `'R'` after `'A'`. Both converge deterministically.
  3. Concurrent duplicate deletions of the same character (idempotency).
  4. Out-of-order delivery: delete arrives before character insertion (buffered and applied).
- **CMake**: Add `collabpad_phase6_demo` target.

#### Task 4.3: End-to-End Integration & Regression Suite
- **Target File**: `tests/test_deletion.py` (New)
- **Verification Cases**:
  1. Client 1 types `"HELLO"`. Client 2 synchronizes `"HELLO"`.
  2. Client 1 deletes `"LL"`. Client 2 receives deletes in real time; both show `"HEO"`.
  3. Client 2 types `"LP"` after `'E'`. Both converge to `"HELPO"`.
  4. Server crash & restart: Reopened room preserves deleted status of tombstones.
  5. Full regression: Run `test_phase5.py` and `collabpad_phase2_demo.exe` to guarantee zero regressions.

---

## Verification Criteria & Acceptance Tests

- [x] `collabpad_phase6_demo.exe` builds and exits with code 0 (all 6 cases passed).
- [x] `python tests/test_deletion.py 8096` passes all deletion, convergence, and persistence tests (7/7 passed).
- [x] `python tests/test_phase5.py 8095` continues to pass with 10/10 tests green.
- [x] `collabpad_phase2_demo.exe` and `collabpad_demo.exe` continue to exit with code 0.
- [x] Deleted characters in browser client no longer ghost back upon subsequent edits.
