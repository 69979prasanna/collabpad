// client/src/hooks/useCollabSocket.js
// Dedicated WebSocket management hook for CollabPad.
// Handles room joining, operation relay, collaborator presence, cursor synchronization,
// bounded exponential backoff reconnection, and missed-operation recovery.

import { useState, useEffect, useRef, useCallback } from 'react';

const COLLAB_COLORS = [
  '#6366f1', // Indigo
  '#06b6d4', // Cyan
  '#10b981', // Emerald
  '#f59e0b', // Amber
  '#ec4899', // Pink
  '#8b5cf6', // Violet
  '#3b82f6', // Blue
  '#14b8a6', // Teal
];

export function useCollabSocket({
  serverUrl = 'ws://127.0.0.1:8080',
  docId = 'default',
  siteId,
  userName = 'Collaborator',
  onRemoteOp,
  onSync,
}) {
  const [connectionStatus, setConnectionStatus] = useState('connecting'); // 'connecting' | 'online' | 'offline'
  const [sessionId, setSessionId] = useState(null);
  const [collaborators, setCollaborators] = useState([]);
  const [remoteCursors, setRemoteCursors] = useState({});
  const [activityFeed, setActivityFeed] = useState([]);

  const wsRef = useRef(null);
  const reconnectTimeoutRef = useRef(null);
  const presenceIntervalRef = useRef(null);
  const pruneIntervalRef = useRef(null);
  const backoffDelayRef = useRef(1000); // Initial 1000ms backoff
  const offlineQueueRef = useRef([]); // Buffer ops created while offline
  const lastCursorSendRef = useRef(0); // Throttle cursor broadcasts

  const onRemoteOpRef = useRef(onRemoteOp);
  onRemoteOpRef.current = onRemoteOp;

  const onSyncRef = useRef(onSync);
  onSyncRef.current = onSync;

  // Pick deterministic color based on siteId
  const userColor = COLLAB_COLORS[(siteId || 1) % COLLAB_COLORS.length];

  const logActivity = useCallback((message, type = 'info') => {
    setActivityFeed(prev => [
      {
        id: Math.random().toString(36).substring(2, 9),
        time: new Date().toLocaleTimeString(),
        message,
        type,
      },
      ...prev.slice(0, 40),
    ]);
  }, []);

  const send = useCallback((payload) => {
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(payload));
      return true;
    }
    return false;
  }, []);

  // Send an RGA operation to the room (buffers if offline)
  const sendOperation = useCallback((op) => {
    const payload = {
      type: 'op',
      docId,
      siteId,
      op,
    };

    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(payload));
      return true;
    } else {
      // Buffer offline operation for safe recovery upon reconnection
      offlineQueueRef.current.push(payload);
      logActivity('Local edit buffered while disconnected', 'offline');
      return false;
    }
  }, [docId, siteId, logActivity]);

  // Broadcast local cursor position (throttled to 50ms)
  const sendCursor = useCallback((cursorPos, status = 'editing') => {
    const now = Date.now();
    if (now - lastCursorSendRef.current < 45) {
      return false; // Throttled to prevent flooding
    }
    lastCursorSendRef.current = now;

    return send({
      type: 'cursor',
      docId,
      siteId,
      name: userName,
      color: userColor,
      cursor: cursorPos,
      status,
    });
  }, [send, docId, siteId, userName, userColor]);

  // Connect and lifecycle with bounded exponential backoff
  useEffect(() => {
    let isCancelled = false;

    function scheduleReconnect() {
      if (isCancelled) return;
      if (reconnectTimeoutRef.current) clearTimeout(reconnectTimeoutRef.current);

      // Bounded exponential backoff: 1s, ~1.8s, ~3.2s, ... up to 15s max with jitter
      const currentDelay = backoffDelayRef.current;
      const jitter = Math.floor(Math.random() * 400);
      const nextDelay = Math.min(15000, Math.floor(currentDelay * 1.8) + jitter);
      backoffDelayRef.current = nextDelay;

      console.log(`[WebSocket] Scheduling reconnection in ${currentDelay}ms...`);
      reconnectTimeoutRef.current = setTimeout(connect, currentDelay);
    }

    function connect() {
      if (isCancelled) return;
      setConnectionStatus('connecting');

      try {
        const ws = new WebSocket(serverUrl);
        wsRef.current = ws;

        ws.onopen = () => {
          if (isCancelled) {
            ws.close();
            return;
          }
          console.log(`[WebSocket] Connected to ${serverUrl}. Joining room "${docId}"...`);
          setConnectionStatus('online');
          backoffDelayRef.current = 1000; // Reset backoff on successful connection

          // Send join message per protocol
          ws.send(JSON.stringify({
            type: 'join',
            docId,
            siteId,
            name: userName,
          }));

          // Announce presence
          ws.send(JSON.stringify({
            type: 'presence',
            docId,
            siteId,
            name: userName,
            color: userColor,
            status: 'online',
          }));

          logActivity(`Connected to room "${docId}"`, 'connect');
        };

        ws.onmessage = (event) => {
          if (isCancelled) return;
          try {
            const data = JSON.parse(event.data);

            if (data.type === 'joined') {
              setSessionId(data.sessionId);
              console.log(`[WebSocket] Successfully joined room "${data.docId}" (Session #${data.sessionId})`);

              // Load persistent document snapshot if present
              if (data.snapshot && onSyncRef.current) {
                onSyncRef.current(data.snapshot);
                logActivity(`Loaded document snapshot (${data.snapshot.characters?.length || 0} characters)`, 'sync');
              }

              // Flush any operations buffered while offline
              if (offlineQueueRef.current.length > 0) {
                console.log(`[WebSocket] Flushing ${offlineQueueRef.current.length} offline operations to server...`);
                while (offlineQueueRef.current.length > 0) {
                  const queuedOp = offlineQueueRef.current.shift();
                  ws.send(JSON.stringify(queuedOp));
                }
                logActivity('Synchronized offline edits with room', 'sync');
              }
            } else if (data.type === 'sync') {
              if (data.docId === docId && data.snapshot && onSyncRef.current) {
                onSyncRef.current(data.snapshot);
                logActivity('Synchronized latest document state from room', 'sync');
              }
            } else if (data.type === 'op') {
              if (data.docId === docId && data.siteId !== siteId) {
                if (onRemoteOpRef.current) {
                  onRemoteOpRef.current(data.op, data.siteId);
                }
                logActivity(`Received edit from Site #${data.siteId}`, 'edit');
              }
            } else if (data.type === 'cursor') {
              if (data.docId === docId && data.siteId !== siteId) {
                setRemoteCursors(prev => ({
                  ...prev,
                  [data.siteId]: {
                    name: data.name || `User ${data.siteId}`,
                    color: data.color || '#6366f1',
                    cursor: data.cursor,
                    status: data.status || 'editing',
                    lastSeen: Date.now(),
                  },
                }));

                // Update collaborator list
                setCollaborators(prev => {
                  const exists = prev.some(c => c.siteId === data.siteId);
                  if (!exists) {
                    return [...prev, {
                      siteId: data.siteId,
                      name: data.name,
                      color: data.color,
                      status: data.status || 'online',
                      lastSeen: Date.now(),
                      isSelf: false,
                    }];
                  }
                  return prev.map(c => c.siteId === data.siteId
                    ? { ...c, status: data.status || c.status, lastSeen: Date.now() }
                    : c
                  );
                });
              }
            } else if (data.type === 'presence') {
              if (data.docId === docId && data.siteId !== siteId) {
                if (data.status === 'offline' || data.event === 'leave') {
                  // Collaborator disconnected or left room
                  setCollaborators(prev => prev.filter(c => c.siteId !== data.siteId));
                  setRemoteCursors(prev => {
                    const next = { ...prev };
                    delete next[data.siteId];
                    return next;
                  });
                  logActivity(`${data.name || `User ${data.siteId}`} left the room`, 'presence');
                } else {
                  // Active presence update
                  setCollaborators(prev => {
                    const filtered = prev.filter(c => c.siteId !== data.siteId);
                    return [...filtered, {
                      siteId: data.siteId,
                      name: data.name,
                      color: data.color,
                      status: data.status || 'online',
                      isSelf: false,
                      lastSeen: Date.now(),
                    }];
                  });
                  logActivity(`${data.name || `User ${data.siteId}`} joined the room`, 'presence');
                }
              }
            }
          } catch (err) {
            console.error('[WebSocket] Error parsing incoming message:', err);
          }
        };

        ws.onerror = (error) => {
          console.warn('[WebSocket] Error encountered:', error);
          setConnectionStatus('offline');
        };

        ws.onclose = () => {
          console.log('[WebSocket] Connection closed.');
          setConnectionStatus('offline');
          setSessionId(null);
          scheduleReconnect();
        };
      } catch (err) {
        console.error('[WebSocket] Connection exception:', err);
        setConnectionStatus('offline');
        scheduleReconnect();
      }
    }

    connect();

    // Heartbeat presence announcement every 8s
    presenceIntervalRef.current = setInterval(() => {
      send({
        type: 'presence',
        docId,
        siteId,
        name: userName,
        color: userColor,
        status: 'online',
      });
    }, 8000);

    // Housekeeping: mark collaborators as idle if inactive > 4s
    pruneIntervalRef.current = setInterval(() => {
      const now = Date.now();
      setCollaborators(prev => prev.map(c => {
        if (c.lastSeen && now - c.lastSeen > 4000 && c.status === 'editing') {
          return { ...c, status: 'idle' };
        }
        return c;
      }));
    }, 2000);

    return () => {
      isCancelled = true;
      if (reconnectTimeoutRef.current) clearTimeout(reconnectTimeoutRef.current);
      if (presenceIntervalRef.current) clearInterval(presenceIntervalRef.current);
      if (pruneIntervalRef.current) clearInterval(pruneIntervalRef.current);
      if (wsRef.current) {
        wsRef.current.close();
      }
    };
  }, [serverUrl, docId, siteId, userName, userColor, send, logActivity]);

  return {
    connectionStatus,
    sessionId,
    collaborators,
    remoteCursors,
    activityFeed,
    userColor,
    sendOperation,
    sendCursor,
  };
}
