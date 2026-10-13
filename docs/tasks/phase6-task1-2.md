# Phase 6 — Task 1.2: C++ Deletion Operations

## Objective
Implement deletion operations and replica handling in the existing C++ RGA engine.

## Acceptance criteria
- [ ] Confirm Task 1.1 is complete before implementation.
- [ ] Add the planned DeletionOp representation.
- [ ] Implement localDelete and applyDelete according to the existing architecture.
- [ ] Handle duplicate deletions safely.
- [ ] Buffer deletions for unknown character IDs and apply them when the corresponding insertion arrives.
- [ ] Preserve deterministic ordering and existing insertion behavior.
- [ ] Add focused C++ regression tests.
- [ ] Run relevant builds and tests and report actual results.
- [ ] Review the full Git diff.

## Out of scope
Snapshot serialization, server relay, WebSocket changes, JavaScript, React, and WebAssembly bindings.

## Safety
Follow AGENTS.md and the approved GSD Phase 6 plan. Preserve existing user changes. Do not commit or push automatically. Stop if Task 1.1 is incomplete or out-of-scope changes are necessary.
