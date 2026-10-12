// client/src/components/CollaboratorsPanel.jsx
import React from 'react';
import { Users, Activity, CircleDot } from 'lucide-react';

export function CollaboratorsPanel({
  collaborators,
  currentSiteId,
  currentUserName,
  currentUserColor,
  activityFeed,
}) {
  return (
    <aside className="right-sidebar">
      {/* Active Collaborators Section */}
      <div className="sidebar-section-header">
        <div style={{ display: 'flex', alignContent: 'center', gap: 6 }}>
          <Users size={14} />
          <span>Active Collaborators</span>
        </div>
        <span style={{ fontSize: '0.72rem', color: 'var(--accent-cyan)' }}>
          {collaborators.length + 1}
        </span>
      </div>

      <ul className="collaborators-list">
        {/* Current User */}
        <li className="collaborator-card">
          <div
            className="collab-avatar"
            style={{ backgroundColor: currentUserColor }}
          >
            {currentUserName ? currentUserName[0].toUpperCase() : 'Y'}
          </div>
          <div className="collab-info">
            <div className="collab-name">
              <span>{currentUserName}</span>
              <span
                style={{
                  fontSize: '0.65rem',
                  backgroundColor: 'rgba(99, 102, 241, 0.2)',
                  color: '#818cf8',
                  padding: '1px 5px',
                  borderRadius: 3,
                }}
              >
                You
              </span>
            </div>
            <div className="collab-site">Site ID: #{currentSiteId}</div>
          </div>
          <CircleDot size={12} color="#10b981" />
        </li>

        {/* Remote Collaborators */}
        {collaborators.map((c) => {
          const isEditing = c.status === 'editing';
          const isIdle = c.status === 'idle';
          const statusText = isEditing ? 'Typing...' : isIdle ? 'Idle' : 'Active';
          const statusColor = isEditing ? '#06b6d4' : isIdle ? '#f59e0b' : '#10b981';

          return (
            <li key={c.siteId} className="collaborator-card">
              <div
                className="collab-avatar"
                style={{ backgroundColor: c.color || '#06b6d4' }}
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
                      backgroundColor: `${statusColor}22`,
                      padding: '1px 5px',
                      borderRadius: 3,
                      marginLeft: 4,
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
        <div style={{ display: 'flex', alignContent: 'center', gap: 6 }}>
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
