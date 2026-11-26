// client/src/components/Background3D.jsx
// Decorative animated backdrop: drifting aurora glows over a receding 3D grid floor.
// Purely visual (aria-hidden, pointer-events: none); all motion is CSS.
import React from 'react';

export function Background3D() {
  return (
    <div className="bg-scene" aria-hidden="true">
      <div className="bg-aurora bg-aurora-1" />
      <div className="bg-aurora bg-aurora-2" />
      <div className="bg-aurora bg-aurora-3" />
      <div className="bg-grid-floor" />
      <div className="bg-cube bg-cube-1">
        <span /><span /><span /><span /><span /><span />
      </div>
      <div className="bg-cube bg-cube-2">
        <span /><span /><span /><span /><span /><span />
      </div>
    </div>
  );
}

// Small 3D cube used as the brand mark and the editor empty state.
export function Cube3D({ className = '', children }) {
  return (
    <div className={`cube3d ${className}`} aria-hidden="true">
      <div className="cube3d-inner">
        <span className="cube3d-face front">{children}</span>
        <span className="cube3d-face back">{children}</span>
        <span className="cube3d-face right" />
        <span className="cube3d-face left" />
        <span className="cube3d-face top" />
        <span className="cube3d-face bottom" />
      </div>
    </div>
  );
}
