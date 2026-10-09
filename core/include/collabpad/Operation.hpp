#pragma once

#include "CharId.hpp"

namespace collabpad {

/**
 * @brief Represents an insertion operation in an RGA-based collaborative sequence.
 * 
 * Each operation contains:
 * - id: The new character's unique CharId (siteId + Lamport timestamp).
 * - afterId: The CharId of the character it is inserted after.
 *            CharId::root() (siteId 0, clock 0) represents an insertion
 *            at the very beginning of the document.
 * - value: The character value being inserted.
 */
struct InsertionOp {
    CharId id;
    CharId afterId{CharId::root()};
    char value{'\0'};

    InsertionOp() = default;

    InsertionOp(CharId charId, CharId parentId, char val)
        : id(charId), afterId(parentId), value(val) {}

    bool operator==(const InsertionOp& other) const {
        return id == other.id && afterId == other.afterId && value == other.value;
    }

    bool operator!=(const InsertionOp& other) const {
        return !(*this == other);
    }
};

// Convenient alias for general operation usage
using Operation = InsertionOp;

} // namespace collabpad
