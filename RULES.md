# CollabPad Agent Execution Rules (RULES.md)

**Status**: Mandatory / Non-Negotiable  
**Applies To**: All AI coding agents, autonomous loops (Ralph Loop), and human contributors.  
**Hierarchy**: These rules supersede all default agent behaviors and workflow prompts.

---

## 1. Core Operating Principles

1. **Inspect before editing**: Always read relevant existing source code, configuration, tests, and documentation before modifying any file. Never assume an implementation is missing without verification.
2. **Make the smallest sufficient change**: Implement the minimal code required to satisfy the approved task.
3. **Never rewrite or reformat unrelated files**: Do not refactor untouched functions, reformat whitespace, rename identifiers, or introduce cosmetic changes in unrelated files.
4. **Preserve existing behavior and public interfaces**: Retain backward compatibility across C++ core structures, JSON snapshot schemas, and WebSocket message contracts unless a breaking change is explicitly approved.
5. **Never overwrite user changes or discard uncommitted work**: Do not run `git reset --hard`, `git clean -fd`, `git checkout`, or stash operations that could discard uncommitted changes.
6. **Never expose, print, commit, or document secrets**: Never include API keys, passwords, bearer tokens, or full sensitive environment strings in commits, artifacts, or chat responses.
7. **Never weaken security or validations to force tests to pass**: Do not bypass input validation, framing checks, or error handling merely to get a passing test.
8. **Never invent test or build results**: Evidence must be empirical. Never claim a build succeeded or a test passed unless the command was actually executed and the exit code was 0.
9. **Add or update tests for changed behavior**: Every functional code change must be accompanied or validated by a focused test.
10. **Run relevant tests and regression checks before claiming completion**: Execute existing test suites (`test_deletion.py`, `test_phase5.py`, C++ demos) to prove zero regressions before marking a task complete.
11. **Report commands that could not be run**: If a command fails or cannot be executed in the current environment, state the exact command, error output, and reason clearly.
12. **Stop and request approval on architectural deviations**: Immediately stop and explain the issue if a task requires an architectural deviation, an external dependency, a protocol schema change, or edits outside the defined scope.
13. **Do not automatically commit, push, merge, or deploy**: Changes must remain in the working tree for review unless explicit permission to commit is granted.
14. **Do not suppress warnings or tests to obtain green status**: Do not comment out assertions, delete tests, or silence compiler errors.
15. **Update project documentation based on actual outcomes**: Only mark tasks complete in `TRACKER.md` or `.planning/STATE.md` when verified by empirical test execution.

---

## 2. Strict Task Boundaries

- **Single Bounded Task**: Work on exactly one approved task at a time.
- **Allowed Scope**: Only edit the files explicitly specified in the approved task plan and their immediate test files.
- **Scope Escalation**: If completing an approved task requires modifying files outside its defined scope:
  1. Stop editing.
  2. Explain why the out-of-scope edit is necessary.
  3. Wait for explicit approval before proceeding.

---

## 3. Evidence Standard for Status Claims

Every factual claim regarding code or project status must use one of the following four evidence tiers:

| Tier | Definition | Requirement |
|---|---|---|
| **VERIFIED** | Independently validated during the current session via direct inspection of source code, configuration, or actual test execution output. | Must cite file path, line numbers, or executed test command and result. |
| **REPORTED** | Stated in historical planning documents, commit messages, or previous logs, but not independently verified during this session. | Must be treated as unconfirmed until verified. |
| **PLANNED** | Defined in an approved design, roadmap, or phase plan, but not yet implemented in code. | Must cite the plan document and target phase. |
| **UNKNOWN** | Insufficient evidence exists in the repository to determine status. | Must be marked as an open question. |

---

## 4. Shell & Environment Discipline

1. **One command per invocation**: Never chain commands with `&&` or `||` (Windows PowerShell does not support these operators).
2. **Do not propose `cd` commands**: Execute commands directly with appropriate relative paths or tool working directory parameters.
3. **No background sleep loops**: Never execute long sleep loops to poll process status; rely on reactive event tools or explicit timeout checks.
4. **Preserve line endings**: Maintain existing file encoding (UTF-8) and line endings (LF/CRLF as present).

---

## 5. Ralph Loop Constraints

When executing within an autonomous execution loop (Ralph Loop):
1. Execute exactly **one task** per iteration.
2. Require explicit acceptance criteria and an empirical test command for that task.
3. Review the complete `git diff` before reporting.
4. Provide a concise completion report stating changed files, commands executed, and verified results.
5. **Never automatically proceed to subsequent tasks or phases**.
6. **Never automatically commit or push**.
