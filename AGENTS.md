# CollabPad — Agent Instructions

## Project goal
Improve the existing CollabPad collaborative editor incrementally. Preserve working functionality and follow the approved GSD plan.

## Before making changes
- Read `.planning/STATE.md` if present.
- Read the relevant `.planning/phases/06-rga-deletion-tombstones/PLAN.md` and `RESEARCH.md`.
- Inspect the actual source code before assuming a feature is missing.
- Check the current Git branch and working-tree status.
- Identify the smallest set of files needed for the task.

## Architecture and compatibility
- Preserve the existing C++ RGA/CRDT engine and operation ordering.
- Preserve pending-buffer behavior and deterministic replica convergence.
- Keep the C++ engine, JavaScript engine, WebAssembly bindings, WebSocket protocol, and React client compatible.
- Preserve existing function signatures and insertion behavior unless a change is explicitly approved.
- Avoid duplicating existing implementations such as `isDeleted` or `markDeleted`.
- Preserve compatibility with snapshots created before deletion support.

## Implementation rules
- Work on one approved task at a time.
- Do not implement later GSD tasks automatically.
- Do not rewrite unrelated working functions or perform broad refactors.
- Never overwrite or discard existing user changes.
- Do not claim tests passed unless they were actually executed.
- Report changed files, commands run, actual test results, and unresolved issues.

## Git safety
- Work on the current development branch unless explicitly instructed otherwise.
- Do not switch branches unexpectedly.
- Do not commit, push, reset, or discard changes without explicit approval.
- Review the complete Git diff before reporting completion.

## Stop conditions
Stop and explain the issue if:
- The approved plan conflicts with the actual code.
- A public interface must change unexpectedly.
- A task requires edits outside its defined scope.
- Existing tests regress.
- An existing user change might be overwritten.

Ask for approval before making an architectural decision outside the approved task.
