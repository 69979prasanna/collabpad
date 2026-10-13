# External Integrations & Protocols

## Network & Wire Protocol Overview

CollabPad uses a custom RFC 6455 WebSocket implementation built entirely over native Windows Sockets 2 (`ws2_32`) and standard Win32 APIs. No external messaging brokers (Kafka, RabbitMQ, Redis) or third-party WebSocket libraries (Boost.Beast, libwebsockets) are used.

```
+-----------------------------------------------------------------------------------+
|                                  Web Client (JS/Wasm)                             |
+-----------------------------------------------------------------------------------+
                                         |
                                         | 1. HTTP 1.1 Upgrade Request
                                         | 2. RFC 6455 Binary Frame Streaming (Masked)
                                         | 3. Application JSON Messages
                                         v
+-----------------------------------------------------------------------------------+
|                        Native WebSocket Server (Winsock2)                         |
|                    - select() Multiplexer & SHA-1 Handshake Hand-off              |
|                    - Session Framing & Frame Unmasking                            |
|                    - DocumentHub Room Isolation & Fanout Broadcasting             |
+-----------------------------------------------------------------------------------+
             |                                                  |
             | In-Memory CRDT Operation                         | Snapshot I/O
             v                                                  v
+-----------------------------+               +-------------------------------------+
|   collabpad::Replica Core   |               |     FileRepository (Disk Storage)   |
|   (RGA Algorithm In-Memory) |               |     - atomic .tmp -> .json replace  |
+-----------------------------+               +-------------------------------------+
```

---

## 1. RFC 6455 WebSocket Handshake & Framing

### Handshake Upgrade Sequence
1. The client issues an HTTP 1.1 `GET` with:
   - `Upgrade: websocket`
   - `Connection: Upgrade`
   - `Sec-WebSocket-Key: <base64-random-key>`
   - `Sec-WebSocket-Version: 13`
2. `WebSocketServer::doHandshake()` extracts `Sec-WebSocket-Key` using case-insensitive header matching (`getHeader()`).
3. Computes the response key per RFC 6455 §4.2.2:
   - Concatenates the client key with the standard GUID: `258EAFA5-E914-47DA-95CA-C5AB0DC85B11`.
   - Computes SHA-1 hash via native C++ `collabpad::Sha1::compute()`.
   - Base64 encodes the 20-byte digest via `collabpad::Sha1::base64Encode()`.
4. Returns `HTTP/1.1 101 Switching Protocols` with `Sec-WebSocket-Accept`.

### Binary Frame Processing
- Implemented in `Session::readFrame()`:
  - Byte 0: `FIN` bit and opcode (`0x1` text frame, `0x8` close frame, `0x9` ping frame).
  - Byte 1: Mask bit (client frames must be masked) and payload length (7-bit, 16-bit extended `0x7E`, or 64-bit extended `0x7F`).
  - Next 4 bytes: 32-bit masking key.
  - Payload bytes: Unmasked in-place using XOR: `payload[i] ^= mask[i % 4]`.
- Implemented in `Session::send()`:
  - Formats unmasked server-to-client text frames (`0x81` FIN + Text opcode).
  - Encodes payload length (supports direct <126 bytes, 2-byte extended 126..65535, and 8-byte extended >65535).

---

## 2. Application-Level JSON Message Protocol

All client-server messages are serialized as UTF-8 JSON text frames.

### Message Specifications

#### 1. `join` (Client → Server)
Sent immediately upon WebSocket connection opening.
```json
{
  "type": "join",
  "docId": "demo",
  "siteId": 142,
  "name": "Engineer-142"
}
```

#### 2. `joined` (Server → Client)
Sent by the server to confirm room entry, deliver the assigned session ID, and supply the authoritative document state.
```json
{
  "type": "joined",
  "docId": "demo",
  "sessionId": 1,
  "snapshot": {
    "docId": "demo",
    "title": "demo",
    "clock": 12,
    "characters": [
      {
        "id": { "siteId": 142, "clock": 1 },
        "afterId": { "siteId": 0, "clock": 0 },
        "value": "H"
      }
    ]
  }
}
```

#### 3. `op` (Bidirectional)
Transmits a single character insertion operation. The server applies the operation to the room's CRDT `Replica`, persists the new state to disk, and broadcasts the message to all other connected clients in the room (non-echo broadcast).
```json
{
  "type": "op",
  "docId": "demo",
  "siteId": 142,
  "op": {
    "id": { "siteId": 142, "clock": 2 },
    "afterId": { "siteId": 142, "clock": 1 },
    "value": "e"
  }
}
```

#### 4. `sync` (Server → Client)
Transmitted when full document synchronization or state catch-up is forced.
```json
{
  "type": "sync",
  "docId": "demo",
  "snapshot": {
    "docId": "demo",
    "title": "demo",
    "clock": 45,
    "characters": [...]
  }
}
```

#### 5. `cursor` (Bidirectional)
Ephemeral cursor coordinates relayed to active collaborators in the same room. Throttled to 45ms–50ms on the client.
```json
{
  "type": "cursor",
  "docId": "demo",
  "siteId": 142,
  "name": "Engineer-142",
  "color": "#6366f1",
  "cursor": { "line": 4, "col": 12, "index": 78 },
  "status": "editing"
}
```

#### 6. `presence` (Bidirectional)
Collaborator heartbeat and status broadcast. Emitted on join, periodic 8s heartbeat, and server disconnect detection.
```json
{
  "type": "presence",
  "docId": "demo",
  "siteId": 142,
  "name": "Engineer-142",
  "color": "#6366f1",
  "status": "online"
}
```
*On client disconnect, the server synthesizes a broadcast with `"status": "offline"` and `"event": "leave"`.*

---

## 3. Storage & Persistence Integration

### `DocumentRepository` Abstraction
- Defined in `server/DocumentRepository.hpp` as a pure virtual interface:
  - `save(const DocumentSnapshot& snapshot) -> bool`
  - `load(const std::string& docId, DocumentSnapshot& out) -> bool`
  - `exists(const std::string& docId) -> bool`
  - `listDocuments() -> std::vector<std::string>`

### `FileRepository` Implementation
- Storage directory: `data/documents/` (default) or configurable via CLI argument.
- File naming: Sanitized doc IDs mapped to `<baseDir>/<docId>.json`. Characters outside `[a-zA-Z0-9_-]` are converted to `_`.
- **Atomic File Writing Pattern**:
  1. Writes JSON payload to `<baseDir>/<docId>.tmp`.
  2. Calls `flush()` on output stream.
  3. Uses Win32 `MoveFileExA(tmpPath, targetPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.
  4. Guarantees that power cuts or process kills never leave half-written corrupt documents on disk.

---

## 4. WebAssembly & Emscripten Integration

- Defined in `wasm/bindings.cpp` using Emscripten Embind (`EMSCRIPTEN_BINDINGS`).
- Exported Symbols:
  - `CharId` value object (`siteId`, `clock`)
  - `InsertionOp` value object (`id`, `afterId`, `value`)
  - `Replica` class (`localInsertAfter`, `localInsertAtBeginning`, `applyOperation`, `getText`, `getSiteId`, `hasPendingOperations`, `getPendingCount`, `getIdAt`, `findIndex`, `size`)
- Build pipeline: `wasm/build_wasm.ps1` runs `emcc` with `-s MODULARIZE=1 -s EXPORT_NAME=CollabPadWasm -s ALLOW_MEMORY_GROWTH=1`.
- Outputs generated into `client/public/`:
  - `collabpad_wasm.js`
  - `collabpad_wasm.wasm`
- Frontend Integration: `client/src/engine/engineBridge.js` probes `window.CollabPadWasm` on startup. If found, it routes operations through compiled C++ WebAssembly; if absent, it transparently executes the identical pure JavaScript RGA engine without UI disruption.

---

## 5. Web Dev Server & Cloudflare Tunnel Integration

- Configured in `client/vite.config.js`:
  - Local port: `3000`.
  - Allowed Hosts: Supports Cloudflare Tunnels via `allowedHosts: ['roots-engineer-move-maintain.trycloudflare.com', '*.trycloudflare.com']` for remote collaborator sharing and testing over HTTPS/WSS tunnels.
