// client/src/components/CollaboratorsPanel.jsx
import React from 'react';
import { Users, Activity, CircleDot, X } from 'lucide-react';

export function CollaboratorsPanel({
  collaborators,
  currentSiteId,
  currentUserName,
  currentUserColor,
  activityFeed,
  isOpen = true,
  onClose,
}) {
  return (
    <aside
      className={`right-sidebar ${!isOpen ? 'collapsed' : ''}`}
      aria-label="Active collaborators and recent operations"
    >
      {/* Drawer Header (Visible on tablet & mobile) */}
      <div className="drawer-header">
        <h3>
          <Users size={16} color="var(--color-info)" />
          <span>Collaboration & Activity</span>
        </h3>
        {onClose && (
          <button
            className="btn btn-ghost btn-icon-only"
            onClick={onClose}
            title="Close panel"
            aria-label="Close panel"
          >
            <X size={18} />
          </button>
        )}
      </div>

      {/* Active Collaborators Section */}
      <div className="sidebar-section-header">
        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          <Users size={14} />
          <span>Active Collaborators</span>
        </div>
        <span style={{ fontSize: '0.72rem', color: 'var(--color-info)', fontWeight: 600 }}>
          {collaborators.length + 1}
        </span>
      </div>

      <ul className="collaborators-list">
        {/* Current User */}
        <li className="collaborator-card">
          <div
            className="collab-avatar"
            style={{ backgroundColor: currentUserColor || 'var(--accent-primary)' }}
          >
            {currentUserName ? currentUserName[0].toUpperCase() : 'Y'}
          </div>
          <div className="collab-info">
            <div className="collab-name">
              <span>{currentUserName}</span>
              <span
                style={{
                  fontSize: '0.65rem',
                  backgroundColor: 'var(--accent-primary-subtle)',
                  color: 'var(--accent-primary)',
                  padding: '1px 5px',
                  borderRadius: 3,
                  fontWeight: 600,
                }}
              >
                You
              </span>
            </div>
            <div className="collab-site">Site ID: #{currentSiteId}</div>
          </div>
          <CircleDot size={12} color="var(--color-success)" />
        </li>

        {/* Remote Collaborators */}
        {collaborators.map((c) => {
          const isEditing = c.status === 'editing';
          const isIdle = c.status === 'idle';
          const statusText = isEditing ? 'Typing...' : isIdle ? 'Idle' : 'Active';
          const statusColor = isEditing
            ? 'var(--color-info)'
            : isIdle
            ? 'var(--color-warning)'
            : 'var(--color-success)';
          const statusBg = isEditing
            ? 'var(--color-info-subtle)'
            : isIdle
            ? 'var(--color-warning-subtle)'
            : 'var(--color-success-subtle)';

          return (
            <li key={c.siteId} className="collaborator-card">
              <div
                className="collab-avatar"
                style={{ backgroundColor: c.color || 'var(--color-collab-secondary)' }}
              >
                {c.name ? c.name[0].toUpperCase() : 'U'}
              </div>
              <div className="collab-info">
                <div className="collab-name">
                  <span>{c.name || `User ${c.siteId}`}</span>
                  <span
                    style={{
                      fontSize: '0.62rem',
                      color: statusColor,
                      backgroundColor: statusBg,
                      padding: '1px 5px',
                      borderRadius: 3,
                      marginLeft: 4,
                      fontWeight: 500,
                    }}
                  >
                    {statusText}
                  </span>
                </div>
                <div className="collab-site">Site ID: #{c.siteId}</div>
              </div>
              <CircleDot size={12} color={statusColor} />
            </li>
          );
        })}
      </ul>

      {/* Operation Activity Stream */}
      <div
        className="sidebar-section-header"
        style={{ borderTop: '1px solid var(--border-subtle)', marginTop: 8 }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          <Activity size={14} />
          <span>Recent Operations</span>
        </div>
      </div>

      <div className="activity-feed">
        {activityFeed.length === 0 ? (
          <div style={{ color: 'var(--text-muted)', textAlign: 'center', padding: '16px 0' }}>
            No operations recorded yet
          </div>
        ) : (
          activityFeed.map((item) => (
            <div key={item.id} className="activity-item">
              <div>{item.message}</div>
              <div className="activity-time">{item.time}</div>
            </div>
          ))
        )}
      </div>
    </aside>
  );
}
