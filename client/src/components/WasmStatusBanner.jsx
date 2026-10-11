// client/src/components/WasmStatusBanner.jsx
import React from 'react';
import { Cpu, Layers, HelpCircle } from 'lucide-react';

export function WasmStatusBanner({ isWasmActive, pendingCount, charCount }) {
  return (
    <div className="engine-banner">
      <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          <Cpu size={14} color={isWasmActive ? '#34d399' : '#818cf8'} />
          <span>Engine:</span>
          <span className={`engine-pill ${isWasmActive ? 'wasm' : 'relay'}`}>
            {isWasmActive ? 'C++ WebAssembly (Active)' : 'Native Protocol Relay (Standalone)'}
          </span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: 6, color: 'var(--text-muted)' }}>
          <Layers size={13} />
          <span>Characters: {charCount}</span>
          {pendingCount > 0 && (
            <span style={{ color: '#fbbf24' }}>| Buffered Pending: {pendingCount}</span>
          )}
        </div>
      </div>

      <div style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: '0.72rem', color: 'var(--text-muted)' }}>
        <span>RGA Concurrency Model</span>
      </div>
    </div>
  );
}
