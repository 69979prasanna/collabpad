# Phase 6: RGA Deletion & Tombstone Tracking — Research & Technical Specification

## Executive Summary
This document provides the mathematical foundation, architectural analysis, component impact analysis, and migration design for implementing character deletions in CollabPad's Replicated Growable Array (RGA) CRDT engine.

Currently, CollabPad only supports character insertions. Deletions made in the UI are never communicated to the CRDT or server, resulting in deleted characters ghosting back upon incoming network updates. Phase 6 resolves this gap while preserving 100% backwards compatibility with all existing Phase 1–5 interfaces and test suites.

---

## 1. RGA Deletion Mechanics: Why Tombstones Are Necessary

### The Concurrent Insert-Delete Race Condition
Consider two replicas, $R_A$ and $R_B$, synchronized with text `"CAT"`:
- Characters: $C$ (`id: 1,1`), $A$ (`id: 1,2`), $T$ (`id: 1,3`).
- **Concurrent edits**:
  - $R_A$ deletes character $A$ (user backspaces $A$).
  - $R_B$ concurrently inserts $R$ after $A$ (intending to type `"CART"`).

**If $R_A$ physically purged $A$ from its memory:**
1. When $R_B$'s operation `insert 'R' after A (id: 1,2)` arrives at $R_A$, $R_A$ cannot find parent node `(1,2)`.
2. The operation $R$ is placed into `pendingOperations_` indefinitely.
3. The replicas permanently desynchronize.

**With Tombstone Marking ($A$ marked `isDeleted = true`):**
1. $R_A$ retains $A$ in its tree structure, but excludes it from visual rendering (`getText()`).
2. When $R_B$'s insertion of $R$ arrives at $R_A$, $R_A$ locates parent node $A$ in `Sequence::elements_`.
3. Sibling traversal places $R$ correctly after $A$.
4. Visual text on $R_A$: `"CRT"` ($C$, deleted $A$, $R$, $T$).
5. Visual text on $R_B$ (after receiving $R_A$'s delete): `"CRT"` ($C$, deleted $A$, $R$, $T$).
6. **Both replicas achieve deterministic convergence.**

---

## 2. Impact Analysis by Component

### Component 1: Core CRDT Library (`core/include/collabpad/`)

#### 1. `Sequence.hpp`
- **`struct Character`**: Add `bool isDeleted{false};`.
- **`Sequence::getText()`**:
  - Currently: iterates all `elements_` and appends `value`.
  - Must change to: append `value` only if `!ch.isDeleted`.
- **`Sequence::getIdAt(size_t visualIndex)`**:
  - Currently: returns `elements_[visualIndex].id`.
  - Must change to: iterate `elements_`, count only non-deleted characters until reaching `visualIndex`, and return that character's `id`.
- **`Sequence::findIndex(const CharId& id)`**:
  - Must return the visual index among non-deleted characters (or `-1` if tombstoned / missing).
- **`Sequence::visibleSize()`**: Returns count of non-deleted characters.
- **`Sequence::markDeleted(const CharId& id) -> bool`**:
  - Finds character matching `id`.
  - If found: sets `isDeleted = true`; returns `true`.
  - If not found or already deleted: returns `false`.

#### 2. `Operation.hpp`
- Retain existing `InsertionOp` unchanged.
- Introduce `DeletionOp`:
  ```cpp
  struct DeletionOp {
      CharId targetId;
      uint32_t siteId{0};
      uint64_t clock{0};

      DeletionOp() = default;
      explicit DeletionOp(CharId target) : targetId(target) {}
      DeletionOp(CharId target, uint32_t site, uint64_t clk)
          : targetId(target), siteId(site), clock(clk) {}

      bool operator==(const DeletionOp& other) const {
          return targetId == other.targetId;
      }
  };
  ```

#### 3. `Replica.hpp`
- Add `localDelete(const CharId& targetId) -> DeletionOp`:
  - Increments Lamport clock via `clock_.tick()`.
  - Marks target deleted in `sequence_`.
  - Returns `DeletionOp(targetId, siteId_, clock)`.
- Add `localDeleteAt(size_t visualIndex) -> DeletionOp`:
  - Resolves `targetId = sequence_.getIdAt(visualIndex)`.
  - Delegates to `localDelete(targetId)`.
- Add `applyDelete(const DeletionOp& op) -> bool`:
  - Updates clock via `clock_.update(op.clock)`.
  - If target exists in `sequence_`: `sequence_.markDeleted(op.targetId)`.
  - If target does not exist yet (out-of-order delete arriving before insert): buffers in `pendingDeletions_` (set of `CharId`). When insertions arrive, check `pendingDeletions_` and apply tombstone immediately.
- Preserve all existing Phase 1 & 2 methods (`localInsertAfter`, `applyOperation`, `localInsert`, etc.).

#### 4. `DocumentSnapshot.hpp`
- Update JSON serialization:
  - Add `"deleted": true/false` to character JSON objects in `"characters"`.
  - Add `"pendingDeletions": [ {"siteId": ..., "clock": ...}, ... ]` array to durably preserve pending deletes whose targets have not arrived over the network.
- Update JSON parser `fromJson`:
  - Extract `"deleted"` if present on character objects (defaulting to `false` if omitted for backwards compatibility).
  - Extract `"pendingDeletions"` array (defaulting to empty if omitted).
- Update `Replica::loadSnapshot`:
  - Restore `pendingDeletions_` buffer from the snapshot, ensuring out-of-order deletes survive server restarts and crash recovery.

---

### Component 2: WebSocket Server & Relay (`server/`)

#### 1. `DocumentHub.cpp`
- In `applyOpJson(DocumentRoom& room, const string& json)`:
  - Check for `"type": "delete"` or `"delete": true` in `op`.
  - If delete: extract `targetId.siteId` and `targetId.clock`, construct `DeletionOp`, and call `room.replica->applyDelete(op)`.
  - Set `room.isDirty = true` and persist snapshot via `FileRepository`.
- In `broadcast()`:
  - Forward delete operations to all other clients in the room (non-echo fanout).

---

### Component 3: Dual-Mode Engine Bridge (`client/src/engine/engineBridge.js`)

- Add `isDeleted` to JavaScript `Character` class.
- Update `Sequence`:
  - `markDeleted(id)`
  - `getText()`: filters out `isDeleted`
  - `getIdAt(visualIndex)`: maps visual index to `CharId` skipping tombstones
  - `size()`: returns visible non-deleted character count
- Update `EngineBridge`:
  - `localDelete(targetId)`
  - `applyOperation(rawOp)`: handles both insertion and deletion operations
  - `pendingDeletions`: buffers deletes for missing parents/targets

---

### Component 4: Web Client UI (`client/src/App.jsx`)

- In `handleLocalTextChange(newText, cursorPos)`:
  - Compute prefix and suffix.
  - Compute deleted text span:
    ```javascript
    const deletedCount = oldText.length - prefix - suffix;
    ```
  - If `deletedCount > 0`:
    - Loop through deleted characters from visual index `prefix + deletedCount - 1` down to `prefix`:
      ```javascript
      const targetId = engineRef.current.getIdAt(i);
      if (targetId && !targetId.isRoot()) {
        const delOp = engineRef.current.localDelete(targetId);
        sendOperation({ type: 'delete', targetId: delOp.targetId });
      }
      ```
  - If `insertedCount > 0`:
    - Insert characters as currently done.

---

### Component 5: WebAssembly Bindings (`wasm/bindings.cpp`)

- Export `DeletionOp` value object to Embind.
- Export `localDelete`, `applyDelete`, `markDeleted` on `Replica`.

---

## 3. Backwards Compatibility Guarantees

1. **Phase 1–5 Tests Untouched**: All existing tests (`collabpad_demo.exe`, `collabpad_phase2_demo.exe`, `test_phase5.py`) only perform insertions and will continue to pass without modification.
2. **Persistence Schema Compatibility**: Older snapshots without `"deleted"` fields parse cleanly with `deleted = false`.
3. **Wire Protocol Compatibility**: Existing insertion operations remain identical. Delete operations carry their own `siteId` and Lamport `clock` distinct from the target character's identifier:
   ```json
   {
     "type": "op",
     "docId": "demo",
     "siteId": 142,
     "op": {
       "type": "delete",
       "siteId": 142,
       "clock": 8,
       "targetId": { "siteId": 142, "clock": 5 }
     }
   }
   ```
