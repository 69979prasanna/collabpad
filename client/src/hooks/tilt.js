// client/src/hooks/tilt.js
// Pointer-driven 3D tilt for small cards. Writes CSS custom properties directly
// on the element (no React re-renders); styling lives in `.tilt` in index.css.

const MAX_TILT_DEG = 8;

function prefersReducedMotion() {
  try {
    return window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  } catch (e) {
    return false;
  }
}

export const tiltHandlers = {
  onPointerMove(e) {
    if (e.pointerType !== 'mouse' || prefersReducedMotion()) return;
    const el = e.currentTarget;
    const rect = el.getBoundingClientRect();
    if (!rect.width || !rect.height) return;
    const px = (e.clientX - rect.left) / rect.width;
    const py = (e.clientY - rect.top) / rect.height;
    el.style.setProperty('--rx', `${((0.5 - py) * MAX_TILT_DEG).toFixed(2)}deg`);
    el.style.setProperty('--ry', `${((px - 0.5) * MAX_TILT_DEG).toFixed(2)}deg`);
    el.style.setProperty('--mx', `${(px * 100).toFixed(1)}%`);
    el.style.setProperty('--my', `${(py * 100).toFixed(1)}%`);
  },
  onPointerLeave(e) {
    const el = e.currentTarget;
    el.style.setProperty('--rx', '0deg');
    el.style.setProperty('--ry', '0deg');
  },
};
