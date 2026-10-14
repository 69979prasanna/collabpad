// client/src/engine/engineBridge.js
// Dual-mode CRDT Engine: Seamlessly loads C++ WebAssembly module if compiled,
// or provides native JavaScript RGA engine conforming to the exact C++ algorithm.

export class CharId {
  constructor(siteId = 0, clock = 0) {
    this.siteId = siteId;
    this.clock = clock;
  }

  static root() {
    return new CharId(0, 0);
  }

  isRoot() {
    return this.siteId === 0 && this.clock === 0;
  }

  equals(other) {
    if (!other) return false;
    return this.siteId === other.siteId && this.clock === other.clock;
  }

  compareTo(other) {
    if (this.clock !== other.clock) {
      return this.clock < other.clock ? -1 : 1;
    }
    if (this.siteId !== other.siteId) {
      return this.siteId < other.siteId ? -1 : 1;
    }
    return 0;
  }

  greaterThan(other) {
    return this.compareTo(other) > 0;
  }
}

export class Character {
  constructor(id, afterId, value, isDeleted = false) {
    this.id = id;
    this.afterId = afterId || CharId.root();
    this.value = value;
    this.isDeleted = Boolean(isDeleted);
  }
}

export class Sequence {
  constructor() {
    this.elements = [];
  }

  contains(id) {
    if (id.isRoot()) return true;
    return this.elements.some(ch => ch.id.equals(id));
  }

  markDeleted(id) {
    if (id.isRoot()) return false;
    for (const ch of this.elements) {
      if (ch.id.equals(id)) {
        if (ch.isDeleted) return false;
        ch.isDeleted = true;
        return true;
      }
    }
    return false;
  }

  isTombstone(id) {
    if (id.isRoot()) return false;
    for (const ch of this.elements) {
      if (ch.id.equals(id)) return ch.isDeleted;
    }
    return false;
  }

  getParentId(id) {
    for (const ch of this.elements) {
      if (ch.id.equals(id)) return ch.afterId;
    }
    return CharId.root();
  }

  getDirectChildOf(id, parentId) {
    let p = this.getParentId(id);
    if (p.equals(parentId)) return id;

    let child = id;
    while (!p.isRoot()) {
      if (p.equals(parentId)) return child;
      child = p;
      p = this.getParentId(child);
    }

    if (p.equals(parentId)) return child;
    return CharId.root();
  }

  findInsertIndex(newId, afterId) {
    let parentIdx = 0;
    if (!afterId.isRoot()) {
      let found = false;
      for (let i = 0; i < this.elements.length; ++i) {
        if (this.elements[i].id.equals(afterId)) {
          parentIdx = i + 1;
          break;
        }
      }
      if (!found) return this.elements.length;
    }

    let i = parentIdx;
    while (i < this.elements.length) {
      const curr = this.elements[i];
      const directChild = this.getDirectChildOf(curr.id, afterId);
      if (directChild.isRoot()) {
        break; // Reached end of parent's subtree
      }
      if (newId.greaterThan(directChild)) {
        break; // Priority insertion before lower-priority sibling branch
      }
      ++i;
    }
    return i;
  }

  insertAfter(id, afterId, value, isDeleted = false) {
    if (this.contains(id)) return false;
    const index = this.findInsertIndex(id, afterId);
    this.elements.splice(index, 0, new Character(id, afterId, value, isDeleted));
    return true;
  }

  getText() {
    return this.elements.filter(c => !c.isDeleted).map(c => c.value).join('');
  }

  size() {
    return this.elements.length;
  }

  visibleSize() {
    return this.elements.filter(c => !c.isDeleted).length;
  }

  getIdAt(visualIndex) {
    let current = 0;
    for (const ch of this.elements) {
      if (!ch.isDeleted) {
        if (current === visualIndex) {
          return ch.id;
        }
        current++;
      }
    }
    return CharId.root();
  }

  findIndex(id) {
    let visualIdx = 0;
    for (const ch of this.elements) {
      if (ch.id.equals(id)) {
        return ch.isDeleted ? -1 : visualIdx;
      }
      if (!ch.isDeleted) {
        visualIdx++;
      }
    }
    return -1;
  }
}

export class EngineBridge {
  constructor(siteId) {
    this.siteId = siteId;
    this.clock = 0;
    this.sequence = new Sequence();
    this.pendingOps = [];
    this.pendingDeletions = [];
    this.wasmInstance = null;
    this.isWasmActive = false;

    this.initWasm();
  }

  async initWasm() {
    if (typeof window !== 'undefined' && window.CollabPadWasm) {
      try {
        const mod = await window.CollabPadWasm();
        this.wasmInstance = new mod.Replica(this.siteId);
        this.isWasmActive = true;
        console.log('[EngineBridge] Initialized native C++ WebAssembly Replica for site', this.siteId);
      } catch (err) {
        console.warn('[EngineBridge] Could not instantiate WASM module, fallback active:', err);
      }
    }
  }

  tick() {
    return ++this.clock;
  }

  updateClock(remoteTime) {
    this.clock = Math.max(this.clock, remoteTime);
  }

  localInsertAfter(afterId, value) {
    const timestamp = this.tick();
    const id = new CharId(this.siteId, timestamp);
    const validAfterId = afterId || CharId.root();

    if (this.isWasmActive && this.wasmInstance) {
      try {
        const op = this.wasmInstance.localInsertAfter(validAfterId, value);
        return {
          id: { siteId: op.id.siteId, clock: op.id.clock },
          afterId: { siteId: op.afterId.siteId, clock: op.afterId.clock },
          value: op.value,
        };
      } catch (e) {
        console.error('[EngineBridge] WASM insert error:', e);
      }
    }

    this.sequence.insertAfter(id, validAfterId, value);
    return {
      id: { siteId: id.siteId, clock: id.clock },
      afterId: { siteId: validAfterId.siteId, clock: validAfterId.clock },
      value,
    };
  }

  localInsertAtBeginning(value) {
    return this.localInsertAfter(CharId.root(), value);
  }

  localDelete(targetId) {
    if (!targetId || targetId.isRoot()) return null;
    const timestamp = this.tick();

    if (this.isWasmActive && this.wasmInstance) {
      try {
        const op = this.wasmInstance.localDelete(targetId);
        return {
          type: 'delete',
          siteId: op.siteId,
          clock: op.clock,
          targetId: { siteId: op.targetId.siteId, clock: op.targetId.clock },
        };
      } catch (e) {
        console.error('[EngineBridge] WASM localDelete error:', e);
      }
    }

    this.sequence.markDeleted(targetId);
    return {
      type: 'delete',
      siteId: this.siteId,
      clock: timestamp,
      targetId: { siteId: targetId.siteId, clock: targetId.clock },
    };
  }

  localDeleteAt(visualIndex) {
    const targetId = this.getIdAt(visualIndex);
    if (!targetId || targetId.isRoot()) return null;
    return this.localDelete(targetId);
  }

  applyDelete(delOp) {
    if (!delOp || !delOp.targetId) return false;
    const targetId = new CharId(delOp.targetId.siteId, delOp.targetId.clock);
    if (targetId.isRoot()) return false;

    if (typeof delOp.clock === 'number') {
      this.updateClock(delOp.clock);
    }

    if (this.isWasmActive && this.wasmInstance) {
      try {
        return this.wasmInstance.applyDelete(delOp);
      } catch (e) {
        console.error('[EngineBridge] WASM applyDelete error:', e);
      }
    }

    if (this.sequence.contains(targetId)) {
      return this.sequence.markDeleted(targetId);
    }

    // Buffer out-of-order delete
    if (!this.pendingDeletions.some(p => p.targetId.equals(targetId))) {
      this.pendingDeletions.push({
        targetId,
        siteId: delOp.siteId || 0,
        clock: delOp.clock || 0,
      });
    }
    return false;
  }

  applyOperation(rawOp) {
    if (!rawOp) return false;
    if (rawOp.type === 'delete' || rawOp.targetId) {
      return this.applyDelete(rawOp);
    }

    const op = {
      id: new CharId(rawOp.id.siteId, rawOp.id.clock),
      afterId: new CharId(rawOp.afterId ? rawOp.afterId.siteId : 0, rawOp.afterId ? rawOp.afterId.clock : 0),
      value: rawOp.value,
    };

    this.updateClock(op.id.clock);

    if (this.isWasmActive && this.wasmInstance) {
      try {
        return this.wasmInstance.applyOperation(op);
      } catch (e) {
        console.error('[EngineBridge] WASM applyOperation error:', e);
      }
    }

    if (this.sequence.contains(op.id)) return false;

    // Buffer if parent has not arrived
    if (!op.afterId.isRoot() && !this.sequence.contains(op.afterId)) {
      if (!this.pendingOps.some(p => p.id.equals(op.id))) {
        this.pendingOps.push(op);
      }
      return false;
    }

    // Insert
    this.sequence.insertAfter(op.id, op.afterId, op.value);
    this.checkPendingDeletion(op.id);

    // Resolve unblocked pending
    this.tryApplyPending();
    return true;
  }

  checkPendingDeletion(id) {
    const idx = this.pendingDeletions.findIndex(p => p.targetId.equals(id));
    if (idx !== -1) {
      this.sequence.markDeleted(id);
      this.pendingDeletions.splice(idx, 1);
    }
  }

  tryApplyPending() {
    let progress = true;
    while (progress) {
      progress = false;
      for (let i = 0; i < this.pendingOps.length; ) {
        const op = this.pendingOps[i];
        if (op.afterId.isRoot() || this.sequence.contains(op.afterId)) {
          this.pendingOps.splice(i, 1);
          this.sequence.insertAfter(op.id, op.afterId, op.value);
          this.checkPendingDeletion(op.id);
          progress = true;
        } else {
          ++i;
        }
      }
    }
  }

  getText() {
    if (this.isWasmActive && this.wasmInstance) {
      try {
        return this.wasmInstance.getText();
      } catch (e) {
        // fallback
      }
    }
    return this.sequence.getText();
  }

  getIdAt(index) {
    if (this.isWasmActive && this.wasmInstance) {
      try {
        return this.wasmInstance.getIdAt(index);
      } catch (e) {}
    }
    return this.sequence.getIdAt(index);
  }

  findIndex(id) {
    if (this.isWasmActive && this.wasmInstance) {
      try {
        return this.wasmInstance.findIndex(id);
      } catch (e) {}
    }
    return this.sequence.findIndex(id);
  }

  size() {
    if (this.isWasmActive && this.wasmInstance) {
      try {
        return this.wasmInstance.size();
      } catch (e) {}
    }
    return this.sequence.size();
  }

  visibleSize() {
    if (this.isWasmActive && this.wasmInstance) {
      try {
        return this.wasmInstance.visibleSize();
      } catch (e) {}
    }
    return this.sequence.visibleSize();
  }

  loadSnapshot(snapshot) {
    if (!snapshot) return;
    if (typeof snapshot.clock === 'number') {
      this.clock = Math.max(this.clock, snapshot.clock);
    }
    if (Array.isArray(snapshot.characters)) {
      this.sequence.elements = snapshot.characters.map(ch => new Character(
        new CharId(ch.id.siteId, ch.id.clock),
        new CharId(ch.afterId ? ch.afterId.siteId : 0, ch.afterId ? ch.afterId.clock : 0),
        ch.value,
        Boolean(ch.deleted || ch.isDeleted)
      ));
    }
    this.pendingOps = [];
    if (Array.isArray(snapshot.pendingDeletions)) {
      this.pendingDeletions = snapshot.pendingDeletions.map(del => ({
        targetId: new CharId(
          del.targetId ? del.targetId.siteId : del.siteId,
          del.targetId ? del.targetId.clock : del.clock
        ),
        siteId: del.siteId || 0,
        clock: del.clock || 0,
      }));
    } else {
      this.pendingDeletions = [];
    }
  }

  mergeSnapshot(snapshot) {
    if (!snapshot || !Array.isArray(snapshot.characters)) return;
    if (typeof snapshot.clock === 'number') {
      this.clock = Math.max(this.clock, snapshot.clock);
    }
    for (const ch of snapshot.characters) {
      const charId = new CharId(ch.id.siteId, ch.id.clock);
      if (!this.sequence.contains(charId)) {
        this.applyOperation(ch);
      }
    }
  }
}
