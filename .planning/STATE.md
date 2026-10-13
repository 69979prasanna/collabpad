# Project State & Memory

## Current Position
- **Active Milestone**: `Milestone 1` completed & verified -> transitioning to `Milestone 2` (Production Hardening & Advanced CRDT).
- **Current Phase**: `Phase 5` completed (Git commit `af162db`).
- **Next Phase in Queue**: `Phase 6` (RGA Deletion & Tombstone Tracking).
- **Codebase Mapping**: Completed & committed (`0d229df` in `.planning/codebase/`).

---

## Architectural Decision Records (ADRs)

### ADR-01: Replicated Growable Array (RGA) CRDT
- **Decision**: Adopt RGA with `(clock, siteId)` ordering rather than Logoot, LSEQ, or Operational Transformation (OT).
- **Rationale**: Eliminates central coordination, guarantees deterministic convergence across network reorderings, and handles concurrent edits after identical parent nodes without lock contention.

### ADR-02: Zero Third-Party C++ Server Architecture
- **Decision**: Implement RFC 6455 framing and SHA-1 hashing from scratch on top of Winsock2 `select()`.
- **Rationale**: Avoids MinGW POSIX thread incompatibilities and complex external dependencies (Boost.Beast, OpenSSL) while producing a compact, single-executable native binary (`collabpad_server.exe`).

### ADR-03: Dual-Mode Frontend Execution
- **Decision**: Support both C++ WebAssembly (via Emscripten Embind) and native JavaScript fallback in `engineBridge.js`.
- **Rationale**: Allows developer productivity and instant browser previews without requiring local Emscripten SDK installation, while providing maximum C++ execution performance when Wasm binaries are compiled.

### ADR-04: Atomic Snapshot Persistence
- **Decision**: Write document snapshots to `.tmp` files first, followed by atomic replace via Win32 `MoveFileExA`.
- **Rationale**: Prevents data corruption during server termination or concurrent crash conditions.

---

## Active Gaps & Priority Focus

1. **Deletion Operations (Phase 6)**: The editor currently only supports inserting text. Deletion/tombstone tracking is the top architectural requirement for Milestone 2.
2. **Synchronous Persistence Overhead (Phase 7)**: Transitioning from per-keystroke full snapshot disk writes to debounced flushing or WAL logging.
3. **Multi-byte UTF-8 Support (Phase 9)**: Upgrading `Character.value` to prevent emoji/accented character splitting.

---

## Quick Reference Commands

- **Build Native**: `cmake -S . -B build -G "MinGW Makefiles" ; cmake --build build`
- **Run Server**: `.\build\collabpad_server.exe 8080`
- **Run Full Test Suite**: `python tests/test_phase5.py 8095`
- **Run Web Client**: `cd client ; npm run dev`
