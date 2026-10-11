# CollabPad — Real-Time Collaborative Workspace

CollabPad is a high-performance, real-time collaborative code and text editor built with a C++ Conflict-Free Replicated Data Type (CRDT) engine, a custom native WebSocket server (RFC 6455 via Winsock2), and a modern React/Vite web application.

---

## Architecture Overview

```
+-------------------------------------------------------------------------------+
|                             CollabPad Web Client                              |
|           (React + Vite, Dual-Mode Engine Bridge, Real-time Cursors)          |
+---------------------------------------+---------------------------------------+
                                        |  WebSocket (RFC 6455 JSON Framing)
                                        v
+-------------------------------------------------------------------------------+
|                       CollabPad Native WebSocket Server                       |
|           (C++17, Winsock2 select() Event Loop, Room Isolation)               |
+---------------------------------------+---------------------------------------+
                                        |  Operation Relay
                                        v
+-------------------------------------------------------------------------------+
|                           C++ CRDT Engine (Core)                              |
|         (RGA Sequence, Lamport Timestamps, Pending Dependency Buffer)         |
+-------------------------------------------------------------------------------+
```

### Components

1. **Core CRDT Engine (`core/include/collabpad/`)**:
   - Header-only C++ library implementing the Replicated Growable Array (RGA) CRDT.
   - Deterministic concurrency resolution based on `(clock, siteId)` ordering.
   - Out-of-order delivery tolerance via an automatic pending operation dependency buffer.

2. **WebSocket Server (`server/`)**:
   - Native C++ single-threaded `select()`-based event loop using Windows Sockets (`ws2_32`).
   - Implements full RFC 6455 handshake, frame masking/unmasking, and document room isolation (`DocumentHub`).

3. **Web Client (`client/`)**:
   - Modern dark engineering-oriented user interface.
   - Live editable code and text editor with dynamic line numbering.
   - Real-time collaborator cursors, presence indicators, and activity stream.
   - Multi-room and multi-document workspace management.

4. **WebAssembly Integration (`wasm/`)**:
   - Emscripten Embind bindings (`wasm/bindings.cpp`) exposing `Replica`, `Sequence`, `CharId`, and `InsertionOp`.
   - Dual-mode frontend bridge that dynamically drives the compiled WebAssembly binary when present.

---

## Prerequisites & Development Environment

- **Operating System**: Windows 10/11
- **C++ Toolchain**: MinGW GCC 6.3.0+ & CMake 3.15+
- **Python**: Python 3.7+ with `websockets` (`pip install websockets`)
- **Node.js**: Node.js 18+ and npm

---

## Quick Start & Verification

### 1. Build the Native Targets (C++)

Open PowerShell in the repository root:
```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

This builds:
- `build/collabpad_demo.exe` (Phase 1 index-based CRDT demo)
- `build/collabpad_phase2_demo.exe` (Phase 2 RGA concurrent engine demo)
- `build/collabpad_server.exe` (Phase 3 native WebSocket server)

---

### 2. Run Verification Demos

#### Phase 1 Demo
```powershell
.\build\collabpad_demo.exe
```
*Verifies basic replica synchronization and convergence.*

#### Phase 2 Demo
```powershell
.\build\collabpad_phase2_demo.exe
```
*Verifies RGA concurrent insertions after identical parents and buffering of out-of-order operations.*

---

### 3. Run the WebSocket Server & Automated Tests

Start the server on port 8080:
```powershell
.\build\collabpad_server.exe 8080
```

In a second PowerShell window, run the test suites:

#### Phase 3 Server Test
```powershell
python tests/test_server.py 8080
```
*Verifies client connections, room routing, non-echo broadcasting, room isolation, and disconnect recovery.*

#### Phase 4 End-to-End Collaboration Test
```powershell
python tests/test_collab_e2e.py 8080
```
*Verifies multi-client concurrent typing, cursor position broadcasting, presence tracking, and room boundaries.*

---

### 4. Run the Collaborative Web Client

In the `client/` directory:
```powershell
cd client
npm install
npm run dev
```

Open your browser at `http://localhost:3000`.

To test real-time collaboration:
1. Open `http://localhost:3000` in two separate browser tabs or an Incognito window.
2. Both tabs will automatically connect to the same room (`demo`).
3. Type text or code in one tab — edits and live collaborator cursors synchronize instantly!
4. Create or join another room to verify room isolation.

---

### 5. Optional WebAssembly Compilation

If the Emscripten SDK (`emsdk`) is installed on your system:
```powershell
.\wasm\build_wasm.ps1
```
This generates `client/public/collabpad_wasm.js` and `client/public/collabpad_wasm.wasm`. The web client will automatically switch to `Engine: C++ WebAssembly (Active)`.
