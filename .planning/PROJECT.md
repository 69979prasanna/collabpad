# Project Context: CollabPad

## Overview
CollabPad is a high-performance, real-time collaborative code and text editing platform. It unites a mathematically rigorous C++17 Conflict-Free Replicated Data Type (CRDT) engine, a native Windows WebSocket relay server (`RFC 6455` over Winsock2), durable document persistence with atomic disk writes, live collaborator presence and cursors, and a modern React 18 web application featuring dual-mode execution (JavaScript engine and C++ WebAssembly).

---

## Core Problem Statement & Vision
Real-time collaborative editing often relies on heavyweight cloud brokers, external operational transformation (OT) engines, or complex third-party SaaS dependencies. 

CollabPad proves that low-latency, deterministic multi-user text collaboration can be achieved with:
1. **Zero External Server Dependencies**: A native, single-binary C++ WebSocket server using standard Windows Sockets 2 (`ws2_32`) and custom RFC 6455 framing and SHA-1 hashing.
2. **Deterministic Concurrency**: A pure Replicated Growable Array (RGA) CRDT that resolves conflicts deterministically via `(Lamport clock, siteId)` ordering, completely eliminating merge collisions and lock contention.
3. **Fault-Tolerant Session Resilience**: Causal dependency buffering for out-of-order operation arrival, client-side bounded exponential backoff reconnection, offline edit buffering, and durable atomic JSON snapshot persistence surviving server restarts.
4. **Dual-Mode Web Client**: A web client capable of executing either compiled C++ WebAssembly or native JavaScript algorithms interchangeably with identical semantics.

---

## Architecture & Technology Stack

| Component | Technology | Primary Responsibilities |
|---|---|---|
| **Core Engine** | C++17 (Header-Only) | RGA sequence management, `CharId` generation, Lamport clock ordering, pending dependency resolution, JSON snapshot parsing. |
| **Relay Server** | C++17 / Winsock2 | Single-threaded `select()` loop, RFC 6455 handshake & binary framing, room routing (`DocumentHub`), session lifecycle. |
| **Persistence** | C++17 / Win32 API | `DocumentRepository` abstraction, `FileRepository` with atomic `.tmp` to `.json` replacement via `MoveFileExA`. |
| **Web Client** | React 18 + Vite | Dark engineering UI, code editor with dynamic line numbering, collaborator cursors, presence indicators, activity log. |
| **WebAssembly** | Emscripten (Embind) | Compiles C++ `Replica` and `Sequence` to WebAssembly module (`collabpad_wasm.js/.wasm`). |
| **Dual-Mode Bridge**| JavaScript (ES6) | Probes for WebAssembly on startup; falls back transparently to native JS RGA engine if Wasm is absent. |
| **Test Suite** | Python 3.7+ (`websockets`)| Integration and regression suites verifying wire framing, concurrency, crash recovery, and room isolation. |

---

## Architectural Principles & Constraints

1. **Header-Only Algorithmic Core**: The core CRDT (`core/include/collabpad/`) must remain decoupled from all networking, file I/O, and platform-specific headers.
2. **Zero Third-Party C++ Dependencies**: The native server relies strictly on Windows APIs (`ws2_32`, `windows.h`) to ensure straightforward compilation with standard MinGW GCC toolchains.
3. **Deterministic Ordering**: All concurrent insertions after identical parents are ordered by higher Lamport clock first, tie-broken by higher site ID.
4. **Crash-Safe Persistence**: Disk updates must never write directly to live document files. All modifications write to temporary files first and atomically replace the destination via `MoveFileExA`.
5. **Non-Blocking Client UI**: React state updates must never lock or stall the browser rendering thread. Complex operations are buffered or handled in refs.
