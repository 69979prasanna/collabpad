// client/src/components/Sidebar.jsx
import React, { useState } from 'react';
import { Plus, FolderGit2, FileText, LogIn, X } from 'lucide-react';

export function Sidebar({
  documents,
  activeDocId,
  onSelectDoc,
  onCreateDoc,
  onJoinRoom,
  isOpen = true,
  onClose,
}) {
  const [showJoinInput, setShowJoinInput] = useState(false);
  const [joinInputId, setJoinInputId] = useState('');

  const handleJoinSubmit = (e) => {
    e.preventDefault();
    if (joinInputId.trim()) {
      onJoinRoom(joinInputId.trim());
      setJoinInputId('');
      setShowJoinInput(false);
      if (onClose) onClose();
    }
  };

  const handleItemClick = (docId) => {
    onSelectDoc(docId);
    if (onClose) onClose();
  };

  return (
    <aside
      className={`left-sidebar ${!isOpen ? 'collapsed' : ''}`}
      aria-label="Workspace and document navigation"
    >
      {/* Drawer Header (Visible on tablet & mobile) */}
      <div className="drawer-header">
        <h3>
          <FolderGit2 size={16} color="var(--accent-primary)" />
          <span>Workspaces</span>
        </h3>
        {onClose && (
          <button
            className="btn btn-ghost btn-icon-only"
            onClick={onClose}
            title="Close navigation"
            aria-label="Close navigation"
          >
            <X size={18} />
          </button>
        )}
      </div>

      {/* Workspaces Section */}
      <div className="sidebar-section-header">
        <span>Workspaces</span>
      </div>
      <div style={{ padding: '0 8px 8px' }}>
        <div className="sidebar-item active" style={{ borderLeft: 'none' }}>
          <div className="sidebar-item-content">
            <FolderGit2 size={15} color="var(--accent-primary)" />
            <span className="sidebar-item-title" style={{ fontWeight: 600 }}>
              Default Workspace
            </span>
          </div>
        </div>
      </div>

      {/* Documents Section */}
      <div className="sidebar-section-header">
        <span>Shared Documents ({documents.length})</span>
        <button
          className="btn btn-ghost btn-sm"
          onClick={onCreateDoc}
          title="Create New Document / Room"
          aria-label="Create New Document"
        >
          <Plus size={15} />
        </button>
      </div>

      <ul className="sidebar-list">
        {documents.map((doc) => {
          const isActive = doc.id === activeDocId;
          const displayTitle = doc.title || `Room #${doc.id}`;
          return (
            <li
              key={doc.id}
              className={`sidebar-item ${isActive ? 'active' : ''}`}
              onClick={() => handleItemClick(doc.id)}
              title={`${displayTitle} (#${doc.id})`}
            >
              <div className="sidebar-item-content">
                <FileText
                  size={14}
                  color={isActive ? 'var(--accent-primary)' : 'var(--text-muted)'}
                />
                <span className="sidebar-item-title">{displayTitle}</span>
              </div>
              <span className="sidebar-item-tag">#{doc.id}</span>
            </li>
          );
        })}
      </ul>

      {/* Join Room Section in footer */}
      <div className="sidebar-footer">
        {showJoinInput ? (
          <form
            onSubmit={handleJoinSubmit}
            style={{ display: 'flex', flexDirection: 'column', gap: 6 }}
          >
            <div style={{ display: 'flex', gap: 4 }}>
              <input
                type="text"
                className="input-field"
                style={{ padding: '5px 8px', fontSize: '0.82rem', flex: 1, minWidth: 0 }}
                placeholder="Enter Room ID"
                value={joinInputId}
                onChange={(e) => setJoinInputId(e.target.value)}
                autoFocus
                aria-label="Enter Room ID to join"
              />
              <button type="submit" className="btn btn-primary btn-sm">
                Join
              </button>
            </div>
            <button
              type="button"
              className="btn btn-ghost btn-sm"
              onClick={() => setShowJoinInput(false)}
              style={{ fontSize: '0.74rem' }}
            >
              Cancel
            </button>
          </form>
        ) : (
          <button
            className="btn btn-secondary btn-sm"
            style={{ width: '100%', justifyContent: 'center' }}
            onClick={() => setShowJoinInput(true)}
          >
            <LogIn size={14} />
            <span>Join Existing Room</span>
          </button>
        )}
      </div>
    </aside>
  );
}
