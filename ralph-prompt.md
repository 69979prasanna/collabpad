# CollabPad Ralph Loop Prompt

Read and follow the repository-root AGENTS.md before making changes.

Follow the approved GSD Phase 6 plan:
- .planning/phases/06-rga-deletion-tombstones/PLAN.md
- .planning/phases/06-rga-deletion-tombstones/RESEARCH.md

## Current objective
Implement only Phase 6 Wave 1 Task 1.2: C++ deletion operations and replica handling, provided Task 1.1 is already complete.

## Required process
1. Inspect Git status and the current branch.
2. Inspect existing Character, Sequence, Operation, Replica, and C++ tests.
3. Verify whether deletion state and markDeleted already exist.
4. Implement only the approved task.
5. Add focused tests for local deletion, duplicate deletion, and out-of-order delivery.
6. Run relevant existing tests and builds.
7. Review the full diff and report actual results.

## Strict restrictions
- Do not rewrite working insertion logic.
- Do not duplicate existing deletion functionality.
- Do not modify snapshots, server code, WebSocket protocol, JavaScript, React, or WebAssembly in this task.
- Do not discard user changes.
- Do not commit, push, reset, or switch branches automatically.
- Do not claim success without running the relevant checks.
- Stop if Task 1.1 is incomplete or the plan requires changes outside this scope.

Finish after this task. Do not automatically proceed to the next phase or task.
