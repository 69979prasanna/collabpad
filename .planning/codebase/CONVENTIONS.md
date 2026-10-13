# Coding Conventions & Design Patterns

## Code Organization & Architecture Patterns

CollabPad spans multiple language ecosystems (C++17, JavaScript/React, and Python). Each sub-system maintains clear architectural patterns and stylistic consistency.

---

## 1. C++17 Conventions (`core/`, `server/`, `wasm/`)

### File Organization & Headers
- **Header Guards**: `#pragma once` is used across all header files.
- **Namespaces**: All core and server types reside in `namespace collabpad`.
- **Header-Only Core**: The core CRDT library under `core/include/collabpad/` contains only header files (`.hpp`), with inline implementations to maximize inlining and ease WebAssembly / test linkage.
- **Forward Declarations**: Classes in `server/` (e.g. `Session`, `DocumentHub`) use forward declarations in header files to minimize compilation coupling.

### Naming Conventions
- **Types & Structs**: `PascalCase` (e.g., `CharId`, `Sequence`, `Replica`, `DocumentSnapshot`, `DocumentHub`, `FileRepository`).
- **Methods & Functions**: `camelCase` (e.g., `localInsertAfter`, `applyOperation`, `getText`, `sanitizeDocId`, `ensureDirectoryExists`).
- **Member Variables**: `camelCase_` with trailing underscore (e.g., `siteId_`, `clock_`, `elements_`, `pendingOperations_`, `baseDir_`).
- **Constants / Magic Numbers**: `UPPER_SNAKE_CASE` or `kCamelCase` (e.g., `MOVEFILE_REPLACE_EXISTING`, `magic` string in SHA-1).

### Memory Management & Lifetime
- **Smart Pointers**:
  - `std::unique_ptr` for exclusive resource ownership (e.g., `DocumentRoom::replica`).
  - `std::shared_ptr` for shared lifetimes across sessions and rooms (e.g., `DocumentHub`, `DocumentRepository`, `Session`).
  - `std::enable_shared_from_this<Session>` for safe shared pointer generation during event callbacks.
- **Raw Descriptors**: Socket descriptors (`SOCKET` / `uintptr_t`) are stored as raw values managed strictly through RAII inside `Session` and `WebSocketServer` destructors.

### Error Handling & Defensive Programming
- **Zero Raw Exceptions in Critical Loops**: The WebSocket server loop traps socket disconnections through return codes (`recv <= 0`), cleanly marks sessions `alive_ = false`, and purges them via `pruneDeadSessions()`.
- **Atomic Operations**: File updates never overwrite target files directly; writes go to `.tmp` files and utilize Win32 `MoveFileExA` with `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`.

---

## 2. JavaScript & React Conventions (`client/`)

### Component Architecture & Organization
- **Functional Components**: 100% React functional components utilizing hooks (`useState`, `useEffect`, `useRef`, `useCallback`, `useMemo`).
- **File & Component Naming**: `PascalCase.jsx` matching the component name (e.g., `Editor.jsx`, `Toolbar.jsx`, `CollaboratorsPanel.jsx`).
- **Hooks Naming**: `camelCase` prefixed with `use` (e.g., `useCollabSocket.js`).
- **Engine Isolation**: `client/src/engine/engineBridge.js` isolates CRDT algorithms into pure ES6 classes (`CharId`, `Character`, `Sequence`, `EngineBridge`) rather than embedding logic inside React components.

### State & Performance Patterns
- **`useRef` for Mutable Engines**: The CRDT replica instance (`engineRef`), socket connection (`wsRef`), and offline queue (`offlineQueueRef`) are held in React refs rather than component state to avoid triggering unnecessary re-renders on high-frequency network packets.
- **Throttling**: High-frequency cursor transmissions are throttled to 45ms–50ms using timestamp comparisons (`lastCursorSendRef`).
- **Bounded Backoff**: Network disconnections trigger jittered exponential backoff capped at 15s (`backoffDelayRef`).

### CSS & Styling Conventions
- **Vanilla CSS Design System**: `client/src/index.css` manages design tokens via CSS custom properties:
  - Surfaces: `--bg-primary: #0b0f19`, `--bg-secondary: #111827`, `--bg-tertiary: #1f2937`.
  - Borders: `--border-subtle: #2d3748`, `--border-focus: #4f46e5`.
  - Accents: `--accent-indigo: #6366f1`, `--accent-cyan: #06b6d4`, `--accent-emerald: #10b981`, `--accent-amber: #f59e0b`.
  - Typography: `--font-sans: 'Inter', -apple-system, sans-serif`, `--font-mono: 'JetBrains Mono', 'Fira Code', monospace`.
- **BEM-Inspired Utility Classes**: Semantic CSS class names (e.g., `.editor-container`, `.editor-gutter`, `.remote-cursor-flag`, `.collaborator-card`).

---

## 3. Python Integration Test Conventions (`tests/`)

### Script Design & Execution
- **Shebang & Encoding**: Every test script starts with:
  ```python
  #!/usr/bin/env python3
  # -*- coding: utf-8 -*-
  # PYTHONIOENCODING=utf-8
  ```
- **UTF-8 Console Wrapper**: Force-wraps standard output streams (`sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', ...)` to ensure ANSI colors and Unicode characters render reliably on Windows command prompts.
- **Self-Contained Verification**: Test scripts support CLI port parameters and spawn/terminate server child processes automatically when testing restart recovery (`start_server_process`, `stop_server_process`).
- **Structured Assertions**: Uses a standardized `check(label, condition, detail)` pattern that increments a global failure counter and displays colorized `[PASS]` / `[FAIL]` indicators.
