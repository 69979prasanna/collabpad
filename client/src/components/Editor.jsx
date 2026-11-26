// client/src/components/Editor.jsx
import React, { useRef, useMemo } from 'react';
import { Cube3D } from './Background3D';

export function Editor({
  text,
  onLocalTextChange,
  remoteCursors = {},
  onCursorChange,
}) {
  const textareaRef = useRef(null);
  const gutterRef = useRef(null);

  // Synchronize scroll between gutter and textarea
  const handleScroll = (e) => {
    if (gutterRef.current) {
      gutterRef.current.scrollTop = e.target.scrollTop;
    }
  };

  // Generate line numbers
  const linesCount = useMemo(() => {
    return Math.max(1, text.split('\n').length);
  }, [text]);

  const lineNumbers = useMemo(() => {
    const lines = [];
    for (let i = 1; i <= linesCount; ++i) {
      lines.push(i);
    }
    return lines;
  }, [linesCount]);

  // Track cursor position
  const updateCursor = () => {
    if (!textareaRef.current || !onCursorChange) return;
    const pos = textareaRef.current.selectionStart || 0;
    const textUpToCursor = text.substring(0, pos);
    const lines = textUpToCursor.split('\n');
    const line = lines.length;
    const col = lines[lines.length - 1].length;

    onCursorChange({ line, col, index: pos });
  };

  const handleChange = (e) => {
    const newText = e.target.value;
    const cursor = e.target.selectionStart;
    onLocalTextChange(newText, cursor);
    updateCursor();
  };

  return (
    <div className="editor-container">
      {/* Line Numbers Gutter */}
      <div className="editor-gutter" ref={gutterRef} aria-hidden="true">
        {lineNumbers.map((num) => (
          <div key={num}>{num}</div>
        ))}
      </div>

      {/* Editor Content Surface */}
      <div className="editor-textarea-wrapper">
        <textarea
          ref={textareaRef}
          className="editor-textarea"
          value={text}
          onChange={handleChange}
          onSelect={updateCursor}
          onKeyUp={updateCursor}
          onClick={updateCursor}
          onScroll={handleScroll}
          spellCheck={false}
          autoCapitalize="off"
          autoComplete="off"
          autoCorrect="off"
          data-gramm="false"
          placeholder="Start typing ordinary text or source code here..."
          aria-label="Code and text collaborative editor"
        />

        {/* Empty state (decorative; clicks pass through to the textarea) */}
        {!text && (
          <div className="editor-empty" aria-hidden="true">
            <div className="hero-copy">
              <div className="hero-eyebrow">
                Real-time
                <br />
                Conflict-free
                <br />
                Together
              </div>
              <h2 className="hero-title">
                <span className="hero-script">Write</span>
                <span className="hero-serif">Every</span>
                <span className="hero-sans">Moment</span>
              </h2>
              <p className="hero-sub">
                Start typing. Every keystroke merges live for everyone in this room.
              </p>
              <div className="hero-stats">
                <div className="hero-stat">
                  <strong>RGA</strong>
                  <span>Conflict-free merge</span>
                </div>
                <div className="hero-stat">
                  <strong>LIVE</strong>
                  <span>Shared cursors</span>
                </div>
                <div className="hero-stat">
                  <strong>OFFLINE</strong>
                  <span>Edits buffered &amp; synced</span>
                </div>
              </div>
            </div>
            <div className="hero-visual">
              <div className="hero-orbit hero-orbit-1" />
              <div className="hero-orbit hero-orbit-2" />
              <Cube3D className="editor-empty-cube" />
            </div>
          </div>
        )}

        {/* Remote Cursors Floating Display */}
        {Object.entries(remoteCursors).map(([siteId, rc]) => {
          if (!rc.cursor) return null;
          // Approximate position: line height 22px, padding top 16px
          const top = 16 + (rc.cursor.line - 1) * 22;
          const left = 16 + (rc.cursor.col || 0) * 8.4; // approx char width in JetBrains Mono

          return (
            <div
              key={siteId}
              className="remote-cursor-flag"
              style={{
                top: `${top}px`,
                left: `${left}px`,
                '--cursor-color': rc.color || '#6366f1',
              }}
            >
              <div className="cursor-line" />
              <div className="cursor-tag">{rc.name}</div>
            </div>
          );
        })}
      </div>
    </div>
  );
}
