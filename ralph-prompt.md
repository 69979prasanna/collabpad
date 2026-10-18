# CollabPad Ralph Loop Prompt

## Mandatory Reading Order
Before performing any work, read the project control documents in this exact sequence:
1. `RULES.md` — Non-negotiable coding rules and task boundary constraints.
2. `PRD.md` and relevant sections of `DESIGN.md` — Product and architectural contracts.
3. `MEMORY.md` — Architectural decisions (ADRs) and repository gotchas.
4. `TRACKER.md` and `.planning/STATE.md` — Active status and authorized task boundary.
5. Active Phase Plan: `.planning/phases/07-asynchronous-debounced-persistence/PLAN.md` and `RESEARCH.md`.

---

## Current Objective: Phase 7 — Wave 1 Task 1.2
**Task**: Decouple Per-Operation Persistence in `applyOpJson` (`server/DocumentHub.cpp`).  
Confirm that Task 1.1 declarations (`firstDirtyTimeMs`, `lastEditTimeMs`, `flushDirty`, `isRoomDirty`) exist in `server/DocumentHub.hpp`.

### Required Scope:
1. Inspect `server/DocumentHub.hpp` to verify Task 1.1 timing fields and declarations.
2. In `server/DocumentHub.cpp`:
   - In `applyOpJson()` for `isDelete`: remove synchronous `repository_->save(...)`; set `room.isDirty = true`; record monotonic `firstDirtyTimeMs` (if zero) and `lastEditTimeMs`.
   - In `applyOpJson()` for insertions: remove synchronous `repository_->save(...)`; set `room.isDirty = true`; record monotonic `firstDirtyTimeMs` (if zero) and `lastEditTimeMs`.
3. Keep changes strictly bounded to `server/DocumentHub.cpp`.

---

## Required Execution Process
1. Inspect `git status` and working tree before making any changes.
2. Verify Task 1.1 declarations are intact.
3. Make the minimal edits required to decouple synchronous writes from `applyOpJson`.
4. Compile native targets: `cmake --build build`.
5. Run regression checks: `python tests/test_deletion.py 8096` and `python tests/test_phase5.py 8095`.
6. Review the full `git diff`.
7. Generate a concise completion report specifying changed files, commands executed, actual test outcomes, and next step.

---

## Strict Restrictions
- **One bounded task only**: Implement only Task 1.2. Do not proceed to Task 1.3 or Wave 2.
- **Do not invent follow-up tasks** or expand scope beyond `server/DocumentHub.cpp`.
- **Do not modify working CRDT core** (`core/include/collabpad/`).
- **Do not alter wire protocol or JSON serialization formats**.
- **Never discard user changes or uncommitted work**.
- **Never commit, push, or reset automatically**.
- Stop and explain if any build fails, tests regress, or out-of-scope edits are needed.
