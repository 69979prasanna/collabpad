// client/src/components/Toolbar.jsx
import React, { useState } from 'react';
import {
  Share2,
  Users,
  FileCode2,
  Copy,
  Check,
  Sun,
  Moon,
  PanelLeftClose,
  PanelLeft,
  PanelRightClose,
  PanelRight,
} from 'lucide-react';

export function Toolbar({
  docTitle,
  setDocTitle,
  docId,
  connectionStatus,
  collaboratorsCount,
  onOpenShare,
  theme = 'dark',
  onToggleTheme,
  isLeftSidebarOpen = true,
  onToggleLeftSidebar,
  isRightSidebarOpen = true,
  onToggleRightSidebar,
}) {
  const [copied, setCopied] = useState(false);

  const handleCopyRoom = () => {
    navigator.clipboard.writeText(docId);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const statusLabel =
    connectionStatus === 'online'
      ? 'Connected'
      : connectionStatus === 'connecting'
      ? 'Connecting...'
      : 'Disconnected';

  return (
    <header className="top-toolbar">
      <div className="toolbar-left">
        {/* Toggle Workspace Sidebar Button */}
        {onToggleLeftSidebar && (
          <button
            className={`panel-toggle-btn ${isLeftSidebarOpen ? 'active' : ''}`}
            onClick={onToggleLeftSidebar}
            title={isLeftSidebarOpen ? 'Hide sidebar (Ctrl+B)' : 'Show sidebar (Ctrl+B)'}
            aria-label={isLeftSidebarOpen ? 'Close sidebar' : 'Open sidebar'}
          >
            {isLeftSidebarOpen ? <PanelLeftClose size={17} /> : <PanelLeft size={17} />}
          </button>
        )}

        <div className="app-brand" title="CollabPad — Deterministic CRDT Workspace">
          <div className="brand-icon">
            <FileCode2 size={16} />
          </div>
          <span className="brand-text">CollabPad</span>
        </div>

        <input
          type="text"
          className="doc-title-input"
          value={docTitle}
          onChange={(e) => setDocTitle(e.target.value)}
          placeholder="Untitled Document"
          title="Click to rename document"
          aria-label="Document Title"
        />

        <div className="room-badge" title={`Active Room: ${docId}`}>
          <span className="room-badge-label">Room:</span>
          <strong>{docId}</strong>
          <button
            className="btn btn-ghost btn-sm"
            onClick={handleCopyRoom}
            title="Copy Room ID"
            aria-label="Copy Room ID"
            style={{ padding: '2px 4px', marginLeft: 2 }}
          >
            {copied ? <Check size={12} color="#10b981" /> : <Copy size={12} />}
          </button>
        </div>
      </div>

      <div className="toolbar-right">
        {/* Theme toggle button immediately to the LEFT of the Connected indicator */}
        <button
          className="theme-toggle-btn"
          onClick={onToggleTheme}
          title={theme === 'dark' ? 'Switch to light theme' : 'Switch to dark theme'}
          aria-label={theme === 'dark' ? 'Switch to light theme' : 'Switch to dark theme'}
        >
          {theme === 'dark' ? <Sun size={15} /> : <Moon size={15} />}
        </button>

        {/* Connection status pill */}
        <div
          className={`connection-status status-${connectionStatus}`}
          title={`WebSocket Status: ${statusLabel}`}
          aria-label={`Status: ${statusLabel}`}
        >
          <span className="status-dot" />
          <span className="connection-status-text">{statusLabel}</span>
        </div>

        {/* Collaborators counter button (toggles Collaborator drawer/panel) */}
        <button
          className={`btn btn-secondary btn-sm collab-counter-btn ${
            isRightSidebarOpen ? 'active' : ''
          }`}
          onClick={onToggleRightSidebar}
          title={`${collaboratorsCount} collaborator(s) online. Click to toggle activity panel.`}
          aria-label="Toggle collaborators panel"
        >
          <Users size={14} />
          <span className="collab-counter-text">{collaboratorsCount} Online</span>
        </button>

        {/* Invite / Share button */}
        <button
          className="btn btn-primary btn-sm"
          onClick={onOpenShare}
          title="Invite collaborators to this room"
          aria-label="Invite Collaborators"
        >
          <Share2 size={14} />
          <span className="invite-btn-text">Invite</span>
        </button>

        {/* Explicit Toggle Right Panel Button on wide screens */}
        {onToggleRightSidebar && (
          <button
            className={`panel-toggle-btn ${isRightSidebarOpen ? 'active' : ''}`}
            onClick={onToggleRightSidebar}
            title={isRightSidebarOpen ? 'Hide activity panel' : 'Show activity panel'}
            aria-label={isRightSidebarOpen ? 'Close activity panel' : 'Open activity panel'}
          >
            {isRightSidebarOpen ? <PanelRightClose size={17} /> : <PanelRight size={17} />}
          </button>
        )}
      </div>
    </header>
  );
}
