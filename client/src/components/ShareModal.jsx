// client/src/components/ShareModal.jsx
import React, { useState } from 'react';
import { X, Copy, Check } from 'lucide-react';

export function ShareModal({ docId, onClose }) {
  const [copiedLink, setCopiedLink] = useState(false);
  const [copiedId, setCopiedId] = useState(false);

  const inviteUrl = `${window.location.origin}${window.location.pathname}?room=${encodeURIComponent(docId)}`;

  const handleCopyLink = () => {
    navigator.clipboard.writeText(inviteUrl);
    setCopiedLink(true);
    setTimeout(() => setCopiedLink(false), 2000);
  };

  const handleCopyId = () => {
    navigator.clipboard.writeText(docId);
    setCopiedId(true);
    setTimeout(() => setCopiedId(false), 2000);
  };

  return (
    <div
      className="modal-overlay"
      onClick={onClose}
      role="dialog"
      aria-modal="true"
      aria-labelledby="modal-share-title"
    >
      <div className="modal-card" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <h2 id="modal-share-title" className="modal-title">
            Invite Collaborators
          </h2>
          <button
            className="btn btn-ghost btn-icon-only"
            onClick={onClose}
            aria-label="Close modal"
          >
            <X size={18} />
          </button>
        </div>

        <div className="modal-body">
          <p style={{ fontSize: '0.88rem', color: 'var(--text-secondary)' }}>
            Share this room link or Room ID with your team. Anyone joining this room will collaborate in real time with deterministic CRDT convergence.
          </p>

          <div className="input-group">
            <label className="input-label">Invitation Link</label>
            <div className="copy-row">
              <input
                type="text"
                className="input-field"
                value={inviteUrl}
                readOnly
                aria-label="Invitation Link"
              />
              <button
                className="btn btn-primary btn-sm"
                onClick={handleCopyLink}
                aria-label="Copy invitation link"
              >
                {copiedLink ? <Check size={14} /> : <Copy size={14} />}
                <span>{copiedLink ? 'Copied' : 'Copy'}</span>
              </button>
            </div>
          </div>

          <div className="input-group">
            <label className="input-label">Room ID</label>
            <div className="copy-row">
              <input
                type="text"
                className="input-field"
                value={docId}
                readOnly
                aria-label="Room ID"
              />
              <button
                className="btn btn-secondary btn-sm"
                onClick={handleCopyId}
                aria-label="Copy Room ID"
              >
                {copiedId ? <Check size={14} /> : <Copy size={14} />}
                <span>{copiedId ? 'Copied' : 'Copy'}</span>
              </button>
            </div>
          </div>

          <div
            style={{
              padding: '12px',
              backgroundColor: 'var(--accent-primary-subtle)',
              border: '1px solid var(--border-subtle)',
              borderRadius: 'var(--radius-sm)',
              fontSize: '0.8rem',
              color: 'var(--text-secondary)',
            }}
          >
            <strong style={{ color: 'var(--accent-primary)' }}>Multi-user testing:</strong> Open this link in an Incognito tab or second browser window to see two simultaneous collaborators typing and watching each other's edits converge!
          </div>
        </div>

        <div style={{ display: 'flex', justifyContent: 'flex-end', gap: 8 }}>
          <button className="btn btn-secondary" onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  );
}
