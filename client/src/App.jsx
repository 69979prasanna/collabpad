// client/src/App.jsx
import React, { useState, useEffect, useRef, useCallback } from 'react';
import { Toolbar } from './components/Toolbar';
import { Sidebar } from './components/Sidebar';
import { CollaboratorsPanel } from './components/CollaboratorsPanel';
import { Editor } from './components/Editor';
import { ShareModal } from './components/ShareModal';
import { WasmStatusBanner } from './components/WasmStatusBanner';
import { useCollabSocket } from './hooks/useCollabSocket';
import { EngineBridge, CharId } from './engine/engineBridge';

// Default documents loaded on first visit
const DEFAULT_DOCUMENTS = [
  { id: 'demo', title: 'Live Collab Demo' },
  { id: 'main-cpp', title: 'main.cpp' },
  { id: 'architecture-notes', title: 'Architecture Specs' },
];

function loadSavedDocuments() {
  try {
    const stored = localStorage.getItem('collabpad_documents');
    if (stored) {
      const parsed = JSON.parse(stored);
      if (Array.isArray(parsed) && parsed.length > 0) {
        const valid = parsed.filter(
          (d) => d && typeof d.id === 'string' && d.id.trim() && typeof d.title === 'string'
        );
        if (valid.length > 0) return valid;
      }
    }
  } catch (err) {
    console.warn('[Storage] Could not load documents from localStorage:', err);
  }
  return DEFAULT_DOCUMENTS;
}

function saveDocuments(docs) {
  try {
    localStorage.setItem('collabpad_documents', JSON.stringify(docs));
    return true;
  } catch (err) {
    console.error('[Storage] Could not save documents to localStorage:', err);
    return false;
  }
}

// Generate or retrieve siteId unique to this browser tab session
function getOrCreateSiteId() {
  const stored = sessionStorage.getItem('collabpad_site_id');
  if (stored) return parseInt(stored, 10);
  const newId = Math.floor(Math.random() * 899) + 100; // e.g., 100-999
  sessionStorage.setItem('collabpad_site_id', newId.toString());
  return newId;
}

function getInitialRoom() {
  try {
    const urlParams = new URLSearchParams(window.location.search);
    const room = urlParams.get('room');
    if (room && room.trim()) return room.trim();
  } catch (e) {}
  return 'demo';
}

function getInitialTheme() {
  try {
    const saved = localStorage.getItem('collabpad_theme');
    if (saved === 'light' || saved === 'dark') return saved;
  } catch (e) {
    console.warn('[Storage] Could not read theme from localStorage:', e);
  }
  return 'dark'; // Dark is default
}

export default function App() {
  const [siteId] = useState(getOrCreateSiteId);
  const [userName] = useState(`Engineer-${siteId}`);
  const [activeDocId, setActiveDocId] = useState(getInitialRoom);
  const [theme, setTheme] = useState(getInitialTheme);
  const [persistenceError, setPersistenceError] = useState(null);

  const [documents, setDocuments] = useState(loadSavedDocuments);
  const [isShareOpen, setIsShareOpen] = useState(false);
  const [text, setText] = useState('');
  const [pendingCount, setPendingCount] = useState(0);

  // Responsive drawer states:
  // Desktop (>=900px): left sidebar open by default
  // Desktop (>=1200px): right sidebar open by default
  // Mobile / Tablet: closed by default so editor is dominant
  const [isLeftSidebarOpen, setIsLeftSidebarOpen] = useState(() => {
    return typeof window !== 'undefined' ? window.innerWidth >= 900 : true;
  });
  const [isRightSidebarOpen, setIsRightSidebarOpen] = useState(() => {
    return typeof window !== 'undefined' ? window.innerWidth >= 1200 : false;
  });

  // Track window size for mobile backdrop handling
  const [isMobileOrTablet, setIsMobileOrTablet] = useState(() => {
    return typeof window !== 'undefined' ? window.innerWidth < 900 : false;
  });

  useEffect(() => {
    const handleResize = () => {
      const isMobile = window.innerWidth < 900;
      setIsMobileOrTablet(isMobile);
    };

    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []);

  // Keyboard shortcut Ctrl+B / Cmd+B to toggle sidebar
  useEffect(() => {
    const handleKeyDown = (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'b') {
        e.preventDefault();
        setIsLeftSidebarOpen((prev) => !prev);
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, []);

  // Apply theme to document element and persist choice
  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
    try {
      localStorage.setItem('collabpad_theme', theme);
    } catch (e) {
      console.warn('[Storage] Could not save theme preference:', e);
    }
  }, [theme]);

  const handleToggleTheme = useCallback(() => {
    setTheme((prev) => (prev === 'dark' ? 'light' : 'dark'));
  }, []);

  // Ensure active room is tracked in documents list
  useEffect(() => {
    setDocuments((prev) => {
      const exists = prev.some((d) => d.id === activeDocId);
      if (!exists) {
        const updated = [...prev, { id: activeDocId, title: `Room #${activeDocId}` }];
        saveDocuments(updated);
        return updated;
      }
      return prev;
    });
  }, [activeDocId]);

  // Reference to current CRDT engine
  const engineRef = useRef(null);
  const isRemoteUpdatingRef = useRef(false);

  // Initialize engine for active document
  useEffect(() => {
    const bridge = new EngineBridge(siteId);
    engineRef.current = bridge;
    setText(bridge.getText());
    setPendingCount(bridge.sequence ? bridge.sequence.size() : 0);
  }, [siteId, activeDocId]);

  // Handler for incoming remote operations
  const handleRemoteOp = useCallback((rawOp, remoteSiteId) => {
    if (!engineRef.current) return;

    isRemoteUpdatingRef.current = true;
    engineRef.current.applyOperation(rawOp);
    const newText = engineRef.current.getText();
    setText(newText);
    setPendingCount(engineRef.current.pendingOps ? engineRef.current.pendingOps.length : 0);
    isRemoteUpdatingRef.current = false;
  }, []);

  // Handler for initial or reconnection document snapshot synchronization
  const handleSync = useCallback(
    (snapshot) => {
      if (!engineRef.current || !snapshot) return;

      isRemoteUpdatingRef.current = true;
      engineRef.current.loadSnapshot(snapshot);
      const newText = engineRef.current.getText();
      setText(newText);
      setPendingCount(engineRef.current.pendingOps ? engineRef.current.pendingOps.length : 0);
      isRemoteUpdatingRef.current = false;

      // Synchronize snapshot title if authoritatively provided by server
      if (snapshot.title && snapshot.title.trim() && snapshot.title !== activeDocId) {
        setDocuments((prev) => {
          const currentDoc = prev.find((d) => d.id === activeDocId);
          if (currentDoc && (currentDoc.title === activeDocId || !currentDoc.title.trim())) {
            const updated = prev.map((d) =>
              d.id === activeDocId ? { ...d, title: snapshot.title } : d
            );
            saveDocuments(updated);
            return updated;
          }
          return prev;
        });
      }
    },
    [activeDocId]
  );

  // WebSocket hook
  const {
    connectionStatus,
    collaborators,
    remoteCursors,
    activityFeed,
    userColor,
    sendOperation,
    sendCursor,
  } = useCollabSocket({
    serverUrl: 'ws://127.0.0.1:8080',
    docId: activeDocId,
    siteId,
    userName,
    onRemoteOp: handleRemoteOp,
    onSync: handleSync,
  });

  // Local text modification handler: converts textarea changes into RGA operations
  const handleLocalTextChange = useCallback(
    (newText, cursorPos) => {
      if (isRemoteUpdatingRef.current || !engineRef.current) return;

      const oldText = text;
      if (newText === oldText) return;

      // Fast character insertion detection
      // Find common prefix
      let prefix = 0;
      while (
        prefix < oldText.length &&
        prefix < newText.length &&
        oldText[prefix] === newText[prefix]
      ) {
        prefix++;
      }

      // Find common suffix
      let suffix = 0;
      while (
        suffix < oldText.length - prefix &&
        suffix < newText.length - prefix &&
        oldText[oldText.length - 1 - suffix] === newText[newText.length - 1 - suffix]
      ) {
        suffix++;
      }

      // Process deletions first
      const deletedCount = oldText.length - prefix - suffix;
      if (deletedCount > 0) {
        for (let i = prefix + deletedCount - 1; i >= prefix; i--) {
          const targetId = engineRef.current.getIdAt(i);
          if (targetId && !targetId.isRoot()) {
            const delOp = engineRef.current.localDelete(targetId);
            if (delOp) {
              sendOperation({
                type: 'delete',
                siteId: delOp.siteId,
                clock: delOp.clock,
                targetId: { siteId: delOp.targetId.siteId, clock: delOp.targetId.clock },
              });
            }
          }
        }
      }

      const insertedText = newText.substring(prefix, newText.length - suffix);

      // Apply insertions
      if (insertedText.length > 0) {
        let currentAfterId =
          prefix > 0 ? engineRef.current.getIdAt(prefix - 1) : CharId.root();

        for (let i = 0; i < insertedText.length; i++) {
          const ch = insertedText[i];
          const op = engineRef.current.localInsertAfter(currentAfterId, ch);
          currentAfterId = new CharId(op.id.siteId, op.id.clock);

          // Transmit over WebSocket
          sendOperation(op);
        }
      }

      setText(newText);
    },
    [text, sendOperation]
  );

  // Active doc title
  const activeDoc = documents.find((d) => d.id === activeDocId) || {
    id: activeDocId,
    title: activeDocId,
  };

  const handleUpdateTitle = useCallback(
    (newTitle) => {
      const trimmed = (newTitle || '').trim();
      if (!trimmed) {
        return;
      }

      const previousDocs = documents;
      const nextDocs = documents.map((d) =>
        d.id === activeDocId ? { ...d, title: newTitle } : d
      );

      const ok = saveDocuments(nextDocs);
      if (!ok) {
        setPersistenceError('Failed to persist document rename to storage.');
        setDocuments(previousDocs); // Rollback on failure
        return;
      }
      setPersistenceError(null);
      setDocuments(nextDocs);
    },
    [documents, activeDocId]
  );

  const handleSelectDoc = useCallback((docId) => {
    setActiveDocId(docId);
    try {
      const url = new URL(window.location);
      url.searchParams.set('room', docId);
      window.history.pushState({}, '', url);
    } catch (e) {}
  }, []);

  const handleCreateDoc = useCallback(() => {
    const newId = `doc-${Math.floor(Math.random() * 9000) + 1000}`;
    const newTitle = `New Document ${newId}`;
    const newDoc = { id: newId, title: newTitle };
    const nextDocs = [...documents, newDoc];

    const ok = saveDocuments(nextDocs);
    if (!ok) {
      setPersistenceError('Failed to persist newly created document to storage.');
      return;
    }
    setPersistenceError(null);
    setDocuments(nextDocs);
    setActiveDocId(newId);

    try {
      const url = new URL(window.location);
      url.searchParams.set('room', newId);
      window.history.pushState({}, '', url);
    } catch (e) {}
  }, [documents]);

  const handleJoinRoom = useCallback((roomId) => {
    const cleanId = (roomId || '').trim();
    if (!cleanId) return;

    setDocuments((prev) => {
      const exists = prev.some((d) => d.id === cleanId);
      if (!exists) {
        const next = [...prev, { id: cleanId, title: `Room #${cleanId}` }];
        saveDocuments(next);
        return next;
      }
      return prev;
    });

    setActiveDocId(cleanId);
    try {
      const url = new URL(window.location);
      url.searchParams.set('room', cleanId);
      window.history.pushState({}, '', url);
    } catch (e) {}
  }, []);

  const handleCloseDrawers = useCallback(() => {
    if (isMobileOrTablet) {
      setIsLeftSidebarOpen(false);
      setIsRightSidebarOpen(false);
    }
  }, [isMobileOrTablet]);

  return (
    <div className="app-container">
      {/* Top Toolbar */}
      <Toolbar
        docTitle={activeDoc.title}
        setDocTitle={handleUpdateTitle}
        docId={activeDocId}
        connectionStatus={connectionStatus}
        collaboratorsCount={collaborators.length + 1}
        onOpenShare={() => setIsShareOpen(true)}
        theme={theme}
        onToggleTheme={handleToggleTheme}
        isLeftSidebarOpen={isLeftSidebarOpen}
        onToggleLeftSidebar={() => setIsLeftSidebarOpen((prev) => !prev)}
        isRightSidebarOpen={isRightSidebarOpen}
        onToggleRightSidebar={() => setIsRightSidebarOpen((prev) => !prev)}
      />

      {/* Persistence Error Notification Banner */}
      {persistenceError && (
        <div className="persistence-error-banner" role="alert">
          <span>{persistenceError}</span>
          <button
            className="btn btn-ghost btn-sm"
            onClick={() => setPersistenceError(null)}
            style={{ color: 'inherit', padding: '2px 8px' }}
          >
            Dismiss
          </button>
        </div>
      )}

      {/* Main Body with Three-Region Layout */}
      <div className="app-body">
        {/* Backdrop for Mobile / Tablet Slide-Out Drawers */}
        {isMobileOrTablet && (isLeftSidebarOpen || isRightSidebarOpen) && (
          <div
            className="drawer-backdrop visible"
            onClick={handleCloseDrawers}
            aria-label="Close menu overlay"
          />
        )}

        {/* Left Sidebar: Workspaces & Shared Documents */}
        <Sidebar
          documents={documents}
          activeDocId={activeDocId}
          onSelectDoc={handleSelectDoc}
          onCreateDoc={handleCreateDoc}
          onJoinRoom={handleJoinRoom}
          isOpen={isLeftSidebarOpen}
          onClose={() => setIsLeftSidebarOpen(false)}
        />

        {/* Center Workspace (Dominant Region) */}
        <main className="editor-workspace">
          <WasmStatusBanner
            isWasmActive={engineRef.current ? engineRef.current.isWasmActive : false}
            pendingCount={engineRef.current ? engineRef.current.pendingOps.length : 0}
            charCount={text ? text.length : 0}
          />

          <Editor
            text={text}
            onLocalTextChange={handleLocalTextChange}
            remoteCursors={remoteCursors}
            onCursorChange={sendCursor}
          />
        </main>

        {/* Right Sidebar: Collaborators & Operations */}
        <CollaboratorsPanel
          collaborators={collaborators}
          currentSiteId={siteId}
          currentUserName={userName}
          currentUserColor={userColor}
          activityFeed={activityFeed}
          isOpen={isRightSidebarOpen}
          onClose={() => setIsRightSidebarOpen(false)}
        />
      </div>

      {/* Share / Invite Modal */}
      {isShareOpen && (
        <ShareModal docId={activeDocId} onClose={() => setIsShareOpen(false)} />
      )}
    </div>
  );
}
