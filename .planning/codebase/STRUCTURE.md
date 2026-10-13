# Repository Structure

## Directory Layout

```
collabpad/
├── CMakeLists.txt                 # Root CMake build configuration
├── README.md                      # Project documentation and quick-start guide
├── .gitignore                     # Git ignore rules for build artifacts and temporary files
├── build/                         # Native CMake build directory (binaries, makefiles)
│   ├── collabpad_demo.exe         # Phase 1 replica demo binary
│   ├── collabpad_phase2_demo.exe  # Phase 2 RGA concurrency demo binary
│   └── collabpad_server.exe       # Phase 3-5 WebSocket server binary
├── core/                          # Core CRDT engine library and demos
│   ├── include/
│   │   └── collabpad/
│   │       ├── CharId.hpp         # Lamport timestamp & site ID struct
│   │       ├── Character.hpp      # Sequence node definition (in Sequence.hpp)
│   │       ├── DocumentSnapshot.hpp # Snapshot serialization & JSON parser
│   │       ├── LamportClock.hpp   # Lamport logical clock tracker
│   │       ├── Operation.hpp      # InsertionOp struct definition
│   │       ├── Replica.hpp        # High-level CRDT replica manager
│   │       └── Sequence.hpp       # RGA sequence container & tree ordering
│   └── tests/
│       ├── phase1_demo.cpp        # Basic replica exchange terminal verification
│       └── phase2_demo.cpp        # RGA concurrent insertions terminal verification
├── server/                        # Native WebSocket relay server & persistence
│   ├── main.cpp                   # Server CLI entry point & signal handling
│   ├── DocumentHub.hpp/.cpp       # Room isolation, presence routing & CRDT persistence
│   ├── DocumentRepository.hpp     # Abstract persistence interface
│   ├── FileRepository.hpp/.cpp    # Atomic temporary-to-JSON file persistence implementation
│   ├── Session.hpp/.cpp           # Per-client WebSocket frame read/write & message dispatch
│   ├── Sha1.hpp                   # Handshake SHA-1 digest & Base64 encoder
│   └── WebSocketServer.hpp/.cpp   # Winsock2 listener & select() event loop
├── client/                        # React 18 + Vite collaborative web application
│   ├── index.html                 # HTML application shell
│   ├── package.json               # Node.js dependencies and scripts
│   ├── package-lock.json          # Dependency lockfile
│   ├── vite.config.js             # Vite configuration with port and tunnel hosts
│   ├── dist/                      # Production client build artifacts
│   └── src/
│       ├── main.jsx               # React DOM entry point
│       ├── App.jsx                # Core application component & layout state
│       ├── index.css              # Complete dark engineering design system
│       ├── components/            # UI components
│       │   ├── CollaboratorsPanel.jsx # Active collaborator cards & activity feed
│       │   ├── Editor.jsx         # Code/text surface, line numbers & floating cursors
│       │   ├── ShareModal.jsx     # Room share dialog & clipboard copy
│       │   ├── Sidebar.jsx        # Document list, active room switch, join modal
│       │   ├── Toolbar.jsx        # Title editor, connection status pill, room badge
│       │   └── WasmStatusBanner.jsx   # Dynamic Wasm vs JS fallback status badge
│       ├── engine/
│       │   └── engineBridge.js    # Dual-mode CRDT engine (Wasm probe + JS RGA)
│       └── hooks/
│           └── useCollabSocket.js # WebSocket hook (backoff, heartbeat, queue, presence)
├── wasm/                          # WebAssembly compilation scripts and bindings
│   ├── bindings.cpp               # Emscripten Embind exports for C++ Replica
│   ├── build_wasm.ps1             # PowerShell compilation script targeting emcc
│   └── README.md                  # WebAssembly integration setup & build guide
├── tests/                         # Automated Python integration & regression tests
│   ├── test_server.py             # Phase 3 server handshake & broadcast test
│   ├── test_collab_e2e.py         # Phase 4 multi-client collaborative typing test
│   └── test_phase5.py             # Phase 5 full persistence & session recovery suite
└── data/                          # Document persistence storage
    ├── documents/                 # Default persistent document JSON snapshots
    ├── test_docs/                 # Temporary directories used during testing
    └── test_phase5/               # Dedicated directory for Phase 5 automated tests
```

---

## Module Boundaries & Responsibilities

### 1. `core/` (CRDT Mathematical Foundation)
- **Zero dependencies** on networking, OS APIs, or third-party libraries.
- Strictly algorithmic: manages immutable character IDs, deterministic tree insertions, Lamport clock updates, and JSON serialization.
- Header-only library consumable by both the native server (`server/`) and WebAssembly bindings (`wasm/`).

### 2. `server/` (Network Transport & Persistence)
- Owns all socket interactions via Windows Sockets 2 (`ws2_32`).
- Translates network frames into domain events dispatched to `DocumentHub`.
- Isolates rooms and handles lifecycle events (connect, join, disconnect, presence decay).
- Manages durable state via `DocumentRepository` implementations without exposing storage mechanics to the WebSocket loop.

### 3. `client/` (User Experience & Dual-Mode Execution)
- Built as an isolated frontend application with independent packaging (`package.json`).
- Communicates with the server exclusively via standard RFC 6455 WebSockets.
- Houses `engineBridge.js`, decoupling UI rendering in `Editor.jsx` from whether CRDT operations are processed via compiled C++ WebAssembly or pure JavaScript.

### 4. `wasm/` (Bridge Infrastructure)
- Glues `core/` to the web platform using Emscripten.
- Produces artifacts consumed by the client's `public/` folder without modifying the core C++ headers.

### 5. `tests/` (Verification & Quality Assurance)
- Black-box integration tests using Python's `asyncio` and `websockets`.
- Validates the entire client-server wire protocol, room boundaries, concurrent edits, and fault-tolerance guarantees from the perspective of external network clients.

---

## Key Entry Points

| Component | Entry File | Execution Command |
|---|---|---|
| **C++ WebSocket Server** | `server/main.cpp` | `build/collabpad_server.exe [port] [dataDir]` |
| **C++ Phase 1 Demo** | `core/tests/phase1_demo.cpp` | `build/collabpad_demo.exe` |
| **C++ Phase 2 Demo** | `core/tests/phase2_demo.cpp` | `build/collabpad_phase2_demo.exe` |
| **Web Client Dev Server** | `client/src/main.jsx` | `cd client && npm run dev` |
| **Web Client Production Build** | `client/vite.config.js` | `cd client && npm run build` |
| **Wasm Compilation Script** | `wasm/build_wasm.ps1` | `.\wasm\build_wasm.ps1` |
| **Server Integration Test** | `tests/test_server.py` | `python tests/test_server.py [port]` |
| **E2E Collaboration Test** | `tests/test_collab_e2e.py` | `python tests/test_collab_e2e.py [port]` |
| **Phase 5 Full Test Suite** | `tests/test_phase5.py` | `python tests/test_phase5.py [port]` |
