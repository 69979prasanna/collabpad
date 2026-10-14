#pragma once

#include "CharId.hpp"
#include "LamportClock.hpp"
#include "Operation.hpp"
#include "Sequence.hpp"
#include "DocumentSnapshot.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace collabpad {

// Phase 1 compatibility struct for index-based operations
struct InsertOp {
    CharId id;
    char value{'\0'};
    size_t index{0};

    bool operator==(const InsertOp& other) const {
        return id == other.id && value == other.value && index == other.index;
    }
};

class Replica {
public:
    explicit Replica(uint32_t siteId)
        : siteId_(siteId), clock_(0) {}

    // =========================================================================
    // Phase 1 Compatibility API
    // =========================================================================

    InsertOp localInsert(size_t index, char value) {
        uint64_t timestamp = clock_.tick();
        CharId id{siteId_, timestamp};

        sequence_.insert(index, id, value);
        return InsertOp{id, value, index};
    }

    InsertOp localInsert(char value) {
        return localInsert(sequence_.size(), value);
    }

    void applyRemoteInsert(const InsertOp& op) {
        clock_.update(op.id.clock);

        if (sequence_.contains(op.id)) {
            return;
        }

        sequence_.insert(op.index, op.id, op.value);
    }

    // =========================================================================
    // Phase 2: RGA-based Operation API
    // =========================================================================

    /**
     * @brief Creates a local insertion operation after a specific parent character ID.
     * @param afterId The ID of the character to insert after (CharId::root() for start of document).
     * @param value The character to insert.
     * @return The created InsertionOp.
     */
    InsertionOp localInsertAfter(const CharId& afterId, char value) {
        uint64_t timestamp = clock_.tick();
        CharId id{siteId_, timestamp};

        sequence_.insertAfter(id, afterId, value);
        return InsertionOp{id, afterId, value};
    }

    /**
     * @brief Creates a local insertion operation at the very beginning of the document.
     */
    InsertionOp localInsertAtBeginning(char value) {
        return localInsertAfter(CharId::root(), value);
    }

    /**
     * @brief Applies an insertion operation (local or remote) to this replica.
     * 
     * If the operation's parent character has not arrived yet, the operation
     * is placed in a minimal pending buffer until its parent dependency is met.
     * Whenever an operation is successfully inserted, the pending buffer is checked
     * and any unblocked operations are automatically applied.
     * 
     * @param op The insertion operation to apply.
     * @return true if immediately applied to sequence, false if buffered or already seen.
     */
    bool applyOperation(const InsertionOp& op) {
        clock_.update(op.id.clock);

        if (sequence_.contains(op.id)) {
            return false; // Already present, idempotent
        }

        // If parent has not arrived yet (and parent is not root), buffer the operation
        if (!op.afterId.isRoot() && !sequence_.contains(op.afterId)) {
            for (const auto& pending : pendingOperations_) {
                if (pending.id == op.id) {
                    return false;
                }
            }
            pendingOperations_.push_back(op);
            return false;
        }

        // Parent is present (or inserting after root)
        sequence_.insertAfter(op.id, op.afterId, op.value);
        checkPendingDeletion(op.id);

        // Try applying any buffered operations that were waiting on this character
        tryApplyPending();

        return true;
    }

    /**
     * @brief Phase 2 overload allowing applyRemoteInsert to accept an InsertionOp.
     */
    bool applyRemoteInsert(const InsertionOp& op) {
        return applyOperation(op);
    }

    // =========================================================================
    // Phase 6: RGA Deletion & Tombstone API
    // =========================================================================

    /**
     * @brief Creates and applies a local deletion operation for the specified character ID.
     * @param targetId The CharId of the character node to tombstone.
     * @return The created DeletionOp.
     */
    DeletionOp localDelete(const CharId& targetId) {
        if (targetId.isRoot()) {
            return DeletionOp{};
        }
        uint64_t timestamp = clock_.tick();
        sequence_.markDeleted(targetId);
        return DeletionOp{targetId, siteId_, timestamp};
    }

    /**
     * @brief Creates and applies a local deletion operation at a 0-based visual index.
     * Resolves the visual index to CharId (skipping tombstones), then calls localDelete.
     * @param visualIndex The 0-based visual index of the character to delete.
     * @return The created DeletionOp (or empty DeletionOp if index out of bounds).
     */
    DeletionOp localDeleteAt(size_t visualIndex) {
        CharId targetId = sequence_.getIdAt(visualIndex);
        if (targetId.isRoot()) {
            return DeletionOp{};
        }
        return localDelete(targetId);
    }

    /**
     * @brief Applies a deletion operation (local or remote) to this replica.
     * Advances the Lamport clock.
     * If the target character is present in the sequence, marks it as a tombstone.
     * If the target character has not yet arrived (out-of-order delivery), buffers
     * the deletion in pendingDeletions_ to be applied once the target is inserted.
     * @param op The deletion operation.
     * @return true if character was immediately tombstoned, false if buffered or already deleted.
     */
    bool applyDelete(const DeletionOp& op) {
        if (op.targetId.isRoot()) {
            return false;
        }
        clock_.update(op.clock);

        if (sequence_.contains(op.targetId)) {
            return sequence_.markDeleted(op.targetId);
        }

        // Target character has not arrived yet: buffer the deletion
        for (const auto& pending : pendingDeletions_) {
            if (pending.targetId == op.targetId) {
                return false; // Already buffered
            }
        }
        pendingDeletions_.push_back(op);
        return false;
    }

    // =========================================================================
    // Inspection & Accessors
    // =========================================================================

    uint32_t getSiteId() const {
        return siteId_;
    }

    std::string getText() const {
        return sequence_.getText();
    }

    size_t visibleSize() const {
        return sequence_.visibleSize();
    }

    bool visibleEmpty() const {
        return sequence_.visibleEmpty();
    }

    const Sequence& getSequence() const {
        return sequence_;
    }

    const LamportClock& getClock() const {
        return clock_;
    }

    bool hasPendingOperations() const {
        return !pendingOperations_.empty();
    }

    size_t getPendingCount() const {
        return pendingOperations_.size();
    }

    const std::vector<InsertionOp>& getPendingOperations() const {
        return pendingOperations_;
    }

    bool hasPendingDeletions() const {
        return !pendingDeletions_.empty();
    }

    size_t getPendingDeletionCount() const {
        return pendingDeletions_.size();
    }

    const std::vector<DeletionOp>& getPendingDeletions() const {
        return pendingDeletions_;
    }

    DocumentSnapshot createSnapshot(const std::string& docId = "",
                                   const std::string& title = "") const {
        return DocumentSnapshot(docId, title, clock_.getTime(), sequence_.getCharacters(), pendingDeletions_);
    }

    void loadSnapshot(const DocumentSnapshot& snapshot) {
        sequence_.loadCharacters(snapshot.characters);
        clock_ = LamportClock(snapshot.clock);
        pendingOperations_.clear();
        pendingDeletions_ = snapshot.pendingDeletions;
    }

    void setClock(uint64_t clk) {
        clock_ = LamportClock(clk);
    }

private:
    /**
     * @brief Scans pending buffer and applies any operations whose parent dependencies
     * have now been met. Repeats until no more progress can be made.
     */
    void tryApplyPending() {
        bool progress = true;
        while (progress) {
            progress = false;
            for (auto it = pendingOperations_.begin(); it != pendingOperations_.end(); ) {
                if (it->afterId.isRoot() || sequence_.contains(it->afterId)) {
                    InsertionOp op = *it;
                    it = pendingOperations_.erase(it);
                    sequence_.insertAfter(op.id, op.afterId, op.value);
                    checkPendingDeletion(op.id);
                    progress = true;
                } else {
                    ++it;
                }
            }
        }
    }

    void checkPendingDeletion(const CharId& id) {
        for (auto it = pendingDeletions_.begin(); it != pendingDeletions_.end(); ++it) {
            if (it->targetId == id) {
                sequence_.markDeleted(id);
                pendingDeletions_.erase(it);
                break;
            }
        }
    }

    uint32_t siteId_;
    LamportClock clock_;
    Sequence sequence_;
    std::vector<InsertionOp> pendingOperations_;
    std::vector<DeletionOp> pendingDeletions_;
};

} // namespace collabpad
