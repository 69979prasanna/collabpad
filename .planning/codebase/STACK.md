# Technology Stack

## Overview
CollabPad is a distributed, real-time collaborative code and text editing system. It combines a high-performance C++17 Conflict-Free Replicated Data Type (CRDT) engine, a native Windows WebSocket server implemented using Winsock2 without third-party networking runtimes, and a modern React 18 web application with optional WebAssembly acceleration.

---

## Core Languages & Runtimes

| Layer | Language / Standard | Runtime / Compiler | Notes |
|---|---|---|---|
| **Core CRDT Engine** | C++17 (`-std=c++17`) | MinGW GCC 6.3.0+ / Clang / MSVC | Header-only library (`core/include/collabpad/`) |
| **WebSocket Server** | C++17 (`-std=c++17`) | MinGW GCC 6.3.0+ | Native binary (`collabpad_server.exe`), single-threaded Winsock2 `select()` |
| **Web Client** | JavaScript (ES Modules, JSX) | Node.js 18+ (Dev), Modern Browsers (ES2020+) | React 18 SPA built with Vite |
| **WebAssembly** | C++17 / Emscripten | Emscripten SDK (`emcc` / `emcmake`) | Compiles `wasm/bindings.cpp` to `collabpad_wasm.js` and `.wasm` |
| **Test Automation** | Python 3.7+ (Python 3.11 tested) | CPython | Asyncio-driven end-to-end integration test suites |

---

## Build Systems & Toolchains

### C++ Native Build
- **CMake**: Minimum version `3.15` (`CMakeLists.txt` configured with `project(CollabPad VERSION 0.1.0 LANGUAGES CXX)`).
- **Generator**: `MinGW Makefiles` on Windows.
- **Link Libraries**: `ws2_32` (Windows Sockets 2 API for networking and `select()` multiplexing).
- **Targets**:
  - `collabpad_core`: Header-only `INTERFACE` target pointing to `core/include`.
  - `collabpad_demo`: Verification executable for Phase 1 replica index insertion.
  - `collabpad_phase2_demo`: Verification executable for Phase 2 RGA concurrent insertions.
  - `collabpad_server`: Phase 3–5 native WebSocket server executable (`server/main.cpp`, `server/WebSocketServer.cpp`, `server/Session.cpp`, `server/DocumentHub.cpp`, `server/FileRepository.cpp`).
  - `collabpad_wasm`: Optional Emscripten target enabled when `EMSCRIPTEN` is detected.

### Web Client Toolchain
- **Package Manager**: `npm` with `package.json` (`package-lock.json` lockfile v3).
- **Bundler & Dev Server**: `vite` version `^5.4.2` with `@vitejs/plugin-react` version `^4.3.1`.
- **Module Format**: Native ES Modules (`"type": "module"` in `package.json`).
- **Server Port**: Configured to port `3000` with Cloudflare Tunnel allowed hosts in `vite.config.js`.

---

## Dependencies & Libraries

### Web Client Dependencies (`client/package.json`)
- **`react` (`^18.3.1`)**: Component lifecycle, state management, and DOM rendering.
- **`react-dom` (`^18.3.1`)**: React web rendering target.
- **`lucide-react` (`^0.344.0`)**: Modern dark engineering iconography (Users, Activity, FolderGit2, FileText, FileCode2, Share2, LogIn, etc.).

### Web Client Dev Dependencies
- **`vite` (`^5.4.2`)**: Fast HMR development server and Rollup-based production bundler.
- **`@vitejs/plugin-react` (`^4.3.1`)**: Babel/SWC-based Fast Refresh plugin for React JSX.

### C++ Server Dependencies
- **Zero Third-Party C++ Dependencies**:
  - Handshake and RFC 6455 framing implemented from scratch in `server/WebSocketServer.cpp` and `server/Session.cpp`.
  - SHA-1 message digest implemented from scratch in `server/Sha1.hpp`.
  - JSON serialization and parsing implemented natively in `core/include/collabpad/DocumentSnapshot.hpp` and `server/Session.cpp`.
- **System Libraries**:
  - `ws2_32` (Windows Sockets 2: `WSAStartup`, `socket`, `bind`, `listen`, `accept`, `select`, `recv`, `send`, `closesocket`).
  - Win32 API (`windows.h`): `CreateDirectoryA`, `MoveFileExA` (atomic temporary file replacement).

### Test Suite Dependencies
- **`websockets` (`>=10.0`, tested `17.1`)**: Python asynchronous WebSocket client library for testing protocol framing, concurrency, and room isolation.
- **Standard Library Modules**: `asyncio`, `json`, `subprocess`, `shutil`, `sys`, `time`, `io`.

---

## Target Platforms & Environment Requirements

| Target | Platform | Prerequisites |
|---|---|---|
| **Server & Demos** | Windows 10 / Windows 11 | MinGW GCC 6.3.0+ or compatible C++17 compiler, CMake 3.15+ |
| **Web Client** | Modern Web Browsers (Chrome, Edge, Firefox, Safari) | Node.js 18+ and npm |
| **WebAssembly** | WebAssembly MVP + Memory Growth | Emscripten SDK (`emsdk`) |
| **Integration Tests** | Windows (PowerShell / Command Prompt) | Python 3.7+ with `websockets` (`pip install websockets`) |
