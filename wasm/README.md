# CollabPad WebAssembly Engine Integration

This directory contains the Emscripten WebAssembly bindings and build configuration for compiling the native C++ CRDT engine (`collabpad::Replica`, `collabpad::Sequence`, `collabpad::CharId`, `collabpad::InsertionOp`) to WebAssembly for browser execution.

## Architecture

```
+-------------------------------------------------------------+
|                      React Web Client                       |
|  (Editor UI, Room Management, Cursors, useCollabSocket)      |
+------------------------------+------------------------------+
                               |
            +------------------v-------------------+
            |         engineBridge.js              |
            | (Dual-mode: WASM / Native C++ Bridge)|
            +------------------+-------------------+
                               |
    +--------------------------+--------------------------+
    | (WASM Available)                                    | (Standalone Server Relay)
    v                                                     v
+-------------------------------+             +---------------------------+
| collabpad_wasm.js + .wasm     |             | Exact RGA Sequence        |
| (Emscripten Embind of C++     |             | Concurrency Engine        |
|  Replica & Sequence classes)  |             | (Interoperable Protocol)  |
+-------------------------------+             +---------------------------+
```

## Setup & Compilation Steps

Since Emscripten (`emcc`) is not part of the standard MinGW distribution, install the official Emscripten SDK:

### 1. Install Emscripten SDK (emsdk)
Open PowerShell and clone the repository:
```powershell
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
.\emsdk install latest
.\emsdk activate latest
.\emsdk_env.ps1
```

### 2. Compile WebAssembly Module
Navigate back to the CollabPad project and run the build script:
```powershell
.\wasm\build_wasm.ps1
```

This compiles `wasm/bindings.cpp` against `core/include/` into:
- `client/public/collabpad_wasm.js`
- `client/public/collabpad_wasm.wasm`

### 3. Verification
When starting the client (`npm run dev`), the status header will automatically detect and report:
`Engine: C++ WebAssembly (Active)`
If not compiled yet, the application gracefully reports:
`Engine: Native Relay (Awaiting WASM build)`
without breaking any collaborative editing or message protocols.
