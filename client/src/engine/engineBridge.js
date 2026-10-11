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
  constructor(id, afterId, value) {
    this.id = id;
    this.afterId = afterId || CharId.root();
    this.value = value;
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
          found = true;
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

  insertAfter(id, afterId, value) {
    if (this.contains(id)) return false;
    const index = this.findInsertIndex(id, afterId);
    this.elements.splice(index, 0, new Character(id, afterId, value));
    return true;
  }

  getText() {
    return this.elements.map(c => c.value).join('');
  }

  size() {
    return this.elements.length;
  }

  getIdAt(index) {
    if (index >= 0 && index < this.elements.length) {
      return this.elements[index].id;
    }
    return CharId.root();
  }

  findIndex(id) {
    return this.elements.findIndex(c => c.id.equals(id));
  }
}

export class EngineBridge {
  constructor(siteId) {
    this.siteId = siteId;
    this.clock = 0;
    this.sequence = new Sequence();
    this.pendingOps = [];
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

  applyOperation(rawOp) {
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

    // Resolve unblocked pending
    this.tryApplyPending();
    return true;
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
}
