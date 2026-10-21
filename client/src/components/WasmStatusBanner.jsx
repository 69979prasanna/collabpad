// client/src/components/WasmStatusBanner.jsx
import React from 'react';
import { Cpu, Layers, GitBranch } from 'lucide-react';

export function WasmStatusBanner({ isWasmActive, pendingCount, charCount }) {
  return (
    <div className="engine-banner">
      <div className="engine-banner-left">
        {/* Engine Type Pill */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          <Cpu size={14} color={isWasmActive ? 'var(--color-success)' : 'var(--accent-primary)'} />
          <span className="engine-banner-label">Engine:</span>
          <span className={`engine-pill ${isWasmActive ? 'wasm' : 'relay'}`}>
            {isWasmActive ? 'C++ WebAssembly' : 'JS Protocol Relay'}
          </span>
        </div>

        {/* Characters & Buffer Metrics */}
        <div className="engine-stat-badge">
          <Layers size={13} color="var(--text-muted)" />
          <span>
            {charCount} <span className="engine-banner-label">chars</span>
          </span>
          {pendingCount > 0 && (
            <span className="engine-pending-badge" title="Operations queued during offline buffering">
              • {pendingCount} queued
            </span>
          )}
        </div>
      </div>

      <div className="engine-banner-right">
        <GitBranch size={13} color="var(--text-muted)" />
        <span>RGA Concurrency Model</span>
      </div>
    </div>
  );
}
