import React from 'react';
import { Share2, Users, FileCode2, Copy, Check, Sun, Moon } from 'lucide-react';

export function Toolbar({
  docTitle,
  setDocTitle,
  docId,
  connectionStatus,
  collaboratorsCount,
  onOpenShare,
  theme = 'dark',
  onToggleTheme,
}) {
  const [copied, setCopied] = React.useState(false);

  const handleCopyRoom = () => {
    navigator.clipboard.writeText(docId);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <header className="top-toolbar">
      <div className="toolbar-left">
        <div className="app-brand">
          <div className="brand-icon">
            <FileCode2 size={18} />
          </div>
          <span>CollabPad</span>
        </div>

        <input
          type="text"
          className="doc-title-input"
          value={docTitle}
          onChange={(e) => setDocTitle(e.target.value)}
          placeholder="Untitled Document"
          title="Click to rename document"
        />

        <div className="room-badge" title="Active Room ID">
          <span>Room:</span>
          <strong>{docId}</strong>
          <button
            className="btn btn-ghost btn-sm"
            onClick={handleCopyRoom}
            title="Copy Room ID"
            style={{ padding: '2px 4px', marginLeft: 4 }}
          >
            {copied ? <Check size={13} color="#10b981" /> : <Copy size={13} />}
          </button>
        </div>
      </div>

      <div className="toolbar-right">
        {/* Dark/Light mode toggle button immediately to the left of Connected status */}
        <button
          className="theme-toggle-btn"
          onClick={onToggleTheme}
          title={theme === 'dark' ? 'Switch to light mode' : 'Switch to dark mode'}
          aria-label={theme === 'dark' ? 'Switch to light mode' : 'Switch to dark mode'}
        >
          {theme === 'dark' ? <Sun size={15} /> : <Moon size={15} />}
        </button>

        {/* Connection status pill */}
        <div className={`connection-status status-${connectionStatus}`}>
          <span className="status-dot"></span>
          <span>
            {connectionStatus === 'online'
              ? 'Connected'
              : connectionStatus === 'connecting'
              ? 'Connecting...'
              : 'Disconnected'}
          </span>
        </div>

        {/* Collaborators counter */}
        <div
          className="btn btn-secondary btn-sm"
          style={{ cursor: 'default' }}
          title={`${collaboratorsCount} collaborator(s) online`}
        >
          <Users size={14} />
          <span>{collaboratorsCount} Online</span>
        </div>

        {/* Invite / Share button */}
        <button className="btn btn-primary btn-sm" onClick={onOpenShare}>
          <Share2 size={14} />
          <span>Invite</span>
        </button>
      </div>
    </header>
  );
}
