// client/src/components/Sidebar.jsx
import React, { useState } from 'react';
import { Plus, FolderGit2, FileText, Hash, LogIn, ChevronRight } from 'lucide-react';

export function Sidebar({
  documents,
  activeDocId,
  onSelectDoc,
  onCreateDoc,
  onJoinRoom,
}) {
  const [showJoinInput, setShowJoinInput] = useState(false);
  const [joinInputId, setJoinInputId] = useState('');

  const handleJoinSubmit = (e) => {
    e.preventDefault();
    if (joinInputId.trim()) {
      onJoinRoom(joinInputId.trim());
      setJoinInputId('');
      setShowJoinInput(false);
    }
  };

  return (
    <aside className="left-sidebar">
      {/* Workspaces Section */}
      <div className="sidebar-section-header">
        <span>Workspaces</span>
      </div>
      <div style={{ padding: '0 8px 8px' }}>
        <div className="sidebar-item active" style={{ borderLeft: 'none' }}>
          <div className="sidebar-item-content">
            <FolderGit2 size={15} color="#6366f1" />
            <span style={{ fontWeight: 600 }}>Default Workspace</span>
          </div>
        </div>
      </div>

      {/* Documents Section */}
      <div className="sidebar-section-header">
        <span>Shared Documents</span>
        <button
          className="btn btn-ghost btn-sm"
          onClick={onCreateDoc}
          title="Create New Document / Room"
        >
          <Plus size={14} />
        </button>
      </div>

      <ul className="sidebar-list">
        {documents.map((doc) => {
          const isActive = doc.id === activeDocId;
          return (
            <li
              key={doc.id}
              className={`sidebar-item ${isActive ? 'active' : ''}`}
              onClick={() => onSelectDoc(doc.id)}
            >
              <div className="sidebar-item-content">
                <FileText size={14} color={isActive ? '#818cf8' : '#94a3b8'} />
                <span>{doc.title || doc.id}</span>
              </div>
              <span style={{ fontSize: '0.7rem', color: 'var(--text-muted)' }}>
                #{doc.id}
              </span>
            </li>
          );
        })}
      </ul>

      {/* Join Room Section in footer */}
      <div className="sidebar-footer">
        {showJoinInput ? (
          <form onSubmit={handleJoinSubmit} style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
            <div style={{ display: 'flex', gap: 4 }}>
              <input
                type="text"
                className="input-field"
                style={{ padding: '4px 8px', fontSize: '0.8rem', flex: 1 }}
                placeholder="Enter Room ID"
                value={joinInputId}
                onChange={(e) => setJoinInputId(e.target.value)}
                autoFocus
              />
              <button type="submit" className="btn btn-primary btn-sm">
                Join
              </button>
            </div>
            <button
              type="button"
              className="btn btn-ghost btn-sm"
              onClick={() => setShowJoinInput(false)}
              style={{ fontSize: '0.72rem' }}
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
