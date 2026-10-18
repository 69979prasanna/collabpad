# CollabPad — Agent Instructions

## Project goal
Improve the existing CollabPad collaborative editor incrementally. Preserve working functionality and follow the approved GSD plan.

## Mandatory Reading Order
Before undertaking any implementation or modification, agents MUST read documents in this exact order:
1. `RULES.md` — Mandatory coding rules, task boundary constraints, and evidence standards.
2. `PRD.md` and relevant sections of `DESIGN.md` — Product requirements, system architecture, component boundaries, and wire contracts.
3. `MEMORY.md` — Confirmed decisions (ADRs), resolved bugs, and repository constraints.
4. `TRACKER.md` and `.planning/STATE.md` — Current verified status, active milestone, and authorized next task.
5. The active approved phase plan (e.g. `.planning/phases/07-asynchronous-debounced-persistence/PLAN.md` and `RESEARCH.md`).

## Before making changes
- Inspect the actual source code and existing tests before assuming a feature or function is missing.
- Check the current Git branch (`git status`) and confirm working tree state.
- Identify the smallest possible set of files needed for the single approved task.
- Ensure the task is explicitly authorized in `TRACKER.md`.

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
