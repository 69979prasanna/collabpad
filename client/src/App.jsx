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

// Generate or retrieve siteId unique to this browser tab session
function getOrCreateSiteId() {
  const stored = sessionStorage.getItem('collabpad_site_id');
  if (stored) return parseInt(stored, 10);
  const newId = Math.floor(Math.random() * 899) + 100; // e.g., 100-999
  sessionStorage.setItem('collabpad_site_id', newId.toString());
  return newId;
}

function getInitialRoom() {
  const urlParams = new URLSearchParams(window.location.search);
  const room = urlParams.get('room');
  return room ? room.trim() : 'demo';
}

export default function App() {
  const [siteId] = useState(getOrCreateSiteId);
  const [userName] = useState(`Engineer-${siteId}`);
  const [activeDocId, setActiveDocId] = useState(getInitialRoom);

  const [documents, setDocuments] = useState([
    { id: 'demo', title: 'Live Collab Demo' },
    { id: 'main-cpp', title: 'main.cpp' },
    { id: 'architecture-notes', title: 'Architecture Specs' },
  ]);

  const [isShareOpen, setIsShareOpen] = useState(false);
  const [text, setText] = useState('');
  const [pendingCount, setPendingCount] = useState(0);

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
    const applied = engineRef.current.applyOperation(rawOp);
    const newText = engineRef.current.getText();
    setText(newText);
    setPendingCount(engineRef.current.sequence ? engineRef.current.sequence.size() : 0);
    isRemoteUpdatingRef.current = false;
  }, []);

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
  });

  // Local text modification handler: converts textarea changes into RGA operations
  const handleLocalTextChange = useCallback((newText, cursorPos) => {
    if (isRemoteUpdatingRef.current || !engineRef.current) return;

    const oldText = text;
    if (newText === oldText) return;

    // Fast character insertion detection
    // Find common prefix
    let prefix = 0;
    while (prefix < oldText.length && prefix < newText.length && oldText[prefix] === newText[prefix]) {
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

    const insertedText = newText.substring(prefix, newText.length - suffix);

    // Apply insertions
    if (insertedText.length > 0) {
      let currentAfterId = prefix > 0 ? engineRef.current.getIdAt(prefix - 1) : CharId.root();

      for (let i = 0; i < insertedText.length; i++) {
        const ch = insertedText[i];
        const op = engineRef.current.localInsertAfter(currentAfterId, ch);
        currentAfterId = new CharId(op.id.siteId, op.id.clock);

        // Transmit over WebSocket
        sendOperation(op);
      }
    }

    setText(newText);
  }, [text, sendOperation]);

  // Active doc title
  const activeDoc = documents.find((d) => d.id === activeDocId) || { id: activeDocId, title: activeDocId };

  const handleUpdateTitle = (newTitle) => {
    setDocuments((prev) =>
      prev.map((d) => (d.id === activeDocId ? { ...d, title: newTitle } : d))
    );
  };

  const handleCreateDoc = () => {
    const newId = `doc-${Math.floor(Math.random() * 9000) + 1000}`;
    const newDoc = { id: newId, title: `New Document ${newId}` };
    setDocuments((prev) => [...prev, newDoc]);
    setActiveDocId(newId);
  };

  const handleJoinRoom = (roomId) => {
    if (!roomId) return;
    const exists = documents.some((d) => d.id === roomId);
    if (!exists) {
      setDocuments((prev) => [...prev, { id: roomId, title: `Room #${roomId}` }]);
    }
    setActiveDocId(roomId);
  };

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
      />

      <div className="app-body">
        {/* Left Sidebar: Workspaces & Documents */}
        <Sidebar
          documents={documents}
          activeDocId={activeDocId}
          onSelectDoc={setActiveDocId}
          onCreateDoc={handleCreateDoc}
          onJoinRoom={handleJoinRoom}
        />

        {/* Center Workspace */}
        <main className="editor-workspace">
          <WasmStatusBanner
            isWasmActive={engineRef.current ? engineRef.current.isWasmActive : false}
            pendingCount={engineRef.current ? engineRef.current.pendingOps.length : 0}
            charCount={text.length}
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
        />
      </div>

      {/* Share / Invite Modal */}
      {isShareOpen && (
        <ShareModal docId={activeDocId} onClose={() => setIsShareOpen(false)} />
      )}
    </div>
  );
}
