// client/src/components/ShareModal.jsx
import React, { useState } from 'react';
import { X, Copy, Check, ExternalLink } from 'lucide-react';

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
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-card" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <h2 className="modal-title">Invite Collaborators</h2>
          <button className="btn btn-ghost btn-sm" onClick={onClose}>
            <X size={18} />
          </button>
        </div>

        <div className="modal-body">
          <p style={{ fontSize: '0.88rem', color: 'var(--text-secondary)' }}>
            Share this room link or Room ID with your team. Anyone in this room can edit code and text together in real-time.
          </p>

          <div className="input-group">
            <label className="input-label">Invitation Link</label>
            <div className="copy-row">
              <input
                type="text"
                className="input-field"
                value={inviteUrl}
                readOnly
              />
              <button className="btn btn-primary btn-sm" onClick={handleCopyLink}>
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
              />
              <button className="btn btn-secondary btn-sm" onClick={handleCopyId}>
                {copiedId ? <Check size={14} /> : <Copy size={14} />}
                <span>{copiedId ? 'Copied' : 'Copy'}</span>
              </button>
            </div>
          </div>

          <div
            style={{
              padding: '12px',
              backgroundColor: 'rgba(99, 102, 241, 0.08)',
              border: '1px solid rgba(99, 102, 241, 0.2)',
              borderRadius: 'var(--radius-sm)',
              fontSize: '0.8rem',
              color: 'var(--text-secondary)',
            }}
          >
            <strong>Pro-tip for testing:</strong> Open this link in a separate browser tab or an Incognito window to simulate two simultaneous collaborators typing and watching each other's edits converge!
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
