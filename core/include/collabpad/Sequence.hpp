#pragma once

#include "CharId.hpp"
#include <string>
#include <vector>

namespace collabpad {

/**
 * @brief Represents a single character node in the replicated document.
 * Stores its unique CharId, parent character ID (afterId), and character value.
 */
struct Character {
    CharId id;
    CharId afterId{CharId::root()};
    char value{'\0'};
    bool isDeleted{false};

    Character() = default;

    Character(CharId charId, char val)
        : id(charId), afterId(CharId::root()), value(val), isDeleted(false) {}

    Character(CharId charId, CharId parentId, char val)
        : id(charId), afterId(parentId), value(val), isDeleted(false) {}

    Character(CharId charId, CharId parentId, char val, bool del)
        : id(charId), afterId(parentId), value(val), isDeleted(del) {}
};

class Sequence {
public:
    Sequence() = default;

    // --- Phase 1 compatibility ---
    void insert(size_t index, CharId id, char value) {
        if (index > elements_.size()) {
            index = elements_.size();
        }
        elements_.insert(elements_.begin() + index, Character{id, value});
    }

    bool contains(const CharId& id) const {
        if (id.isRoot()) {
            return true; // Virtual root is always present
        }
        for (const auto& ch : elements_) {
            if (ch.id == id) {
                return true;
            }
        }
        return false;
    }

    bool hasId(const CharId& id) const {
        return contains(id);
    }

    /**
     * @brief Returns the visible text of the sequence, excluding deleted characters.
     */
    std::string getText() const {
        std::string text;
        text.reserve(elements_.size());
        for (const auto& ch : elements_) {
            if (!ch.isDeleted) {
                text.push_back(ch.value);
            }
        }
        return text;
    }

    /**
     * @brief Total number of character nodes (including tombstones) in the RGA.
     */
    size_t size() const {
        return elements_.size();
    }

    /**
     * @brief Number of visible (non-deleted) characters in the document.
     */
    size_t visibleSize() const {
        size_t count = 0;
        for (const auto& ch : elements_) {
            if (!ch.isDeleted) {
                ++count;
            }
        }
        return count;
    }

    bool empty() const {
        return elements_.empty();
    }

    bool visibleEmpty() const {
        return visibleSize() == 0;
    }

    const std::vector<Character>& getCharacters() const {
        return elements_;
    }

    // --- Phase 2: RGA-based methods ---

    /**
     * @brief Gets the parent (afterId) of a character by its CharId.
     * Returns CharId::root() if not found or if character has no parent.
     */
    CharId getParentId(const CharId& id) const {
        for (const auto& ch : elements_) {
            if (ch.id == id) {
                return ch.afterId;
            }
        }
        return CharId::root();
    }

    /**
     * @brief Finds the direct child of parentId that is on the ancestor path of id.
     * 
     * If id is in the subtree of parentId, returns the specific sibling (direct child
     * of parentId) that leads to id. If id does not belong to parentId's subtree,
     * returns CharId::root().
     */
    CharId getDirectChildOf(const CharId& id, const CharId& parentId) const {
        auto p = getParentId(id);
        if (p == parentId) {
            return id;
        }

        CharId child = id;
        while (!p.isRoot()) {
            if (p == parentId) {
                return child;
            }
            child = p;
            p = getParentId(child);
        }

        if (p == parentId) { // Both reached root
            return child;
        }

        return CharId::root();
    }

    /**
     * @brief Determines the deterministic insertion index for a new character.
     * 
     * Deterministic ordering rule:
     * 1. The insertion position starts after its parent (or position 0 if inserting after root).
     * 2. Scans forward through any existing children/descendants of the parent:
     *    - If an existing branch of the parent has a direct child with a LOWER CharId
     *      than newId, newId has higher priority, so newId is inserted BEFORE that branch.
     *    - If the branch has a direct child with a HIGHER CharId than newId, newId has
     *      lower priority, so newId must skip that character and all its descendants.
     *    - When we encounter a character that is NOT a descendant of parentId, we stop
     *      because we have reached the end of the parent's subtree.
     */
    size_t findInsertIndex(const CharId& newId, const CharId& afterId) const {
        size_t parentIdx = 0;
        if (!afterId.isRoot()) {
            bool found = false;
            for (size_t i = 0; i < elements_.size(); ++i) {
                if (elements_[i].id == afterId) {
                    parentIdx = i + 1;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return elements_.size();
            }
        } else {
            parentIdx = 0;
        }

        size_t i = parentIdx;
        while (i < elements_.size()) {
            const auto& curr = elements_[i];
            CharId directChild = getDirectChildOf(curr.id, afterId);
            if (directChild.isRoot()) {
                // curr is outside the subtree of afterId; stop here.
                break;
            }
            // directChild is the sibling (direct child of afterId)
            if (newId > directChild) {
                // newId has higher priority than this sibling branch, insert before it.
                break;
            }
            // newId has lower priority, advance past curr
            ++i;
        }
        return i;
    }

    /**
     * @brief Inserts a character after a specified parent ID using RGA ordering.
     */
    bool insertAfter(const CharId& id, const CharId& afterId, char value, bool isDeleted = false) {
        if (contains(id)) {
            return false;
        }
        size_t index = findInsertIndex(id, afterId);
        elements_.insert(elements_.begin() + index, Character{id, afterId, value, isDeleted});
        return true;
    }

    /**
     * @brief Marks a character as deleted (tombstone).
     * Idempotent: returns true if newly marked deleted, false if not found or already deleted.
     */
    bool markDeleted(const CharId& id) {
        if (id.isRoot()) {
            return false;
        }
        for (auto& ch : elements_) {
            if (ch.id == id) {
                if (ch.isDeleted) {
                    return false; // Already deleted
                }
                ch.isDeleted = true;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Checks whether a character has been marked as a tombstone.
     */
    bool isTombstone(const CharId& id) const {
        if (id.isRoot()) {
            return false;
        }
        for (const auto& ch : elements_) {
            if (ch.id == id) {
                return ch.isDeleted;
            }
        }
        return false;
    }

    /**
     * @brief Gets the CharId of the character at the specified 0-based VISIBLE index,
     * skipping any deleted characters (tombstones).
     */
    CharId getIdAt(size_t visualIndex) const {
        size_t current = 0;
        for (const auto& ch : elements_) {
            if (!ch.isDeleted) {
                if (current == visualIndex) {
                    return ch.id;
                }
                ++current;
            }
        }
        return CharId::root();
    }

    /**
     * @brief Gets the CharId of the character at the specified raw physical vector index.
     */
    CharId getPhysicalIdAt(size_t physicalIndex) const {
        if (physicalIndex < elements_.size()) {
            return elements_[physicalIndex].id;
        }
        return CharId::root();
    }

    /**
     * @brief Finds the 0-based VISIBLE index of a character by its CharId.
     * Returns -1 if the character is not found or has been marked as a tombstone.
     */
    int findIndex(const CharId& id) const {
        int visualIdx = 0;
        for (const auto& ch : elements_) {
            if (ch.id == id) {
                return ch.isDeleted ? -1 : visualIdx;
            }
            if (!ch.isDeleted) {
                ++visualIdx;
            }
        }
        return -1;
    }

    /**
     * @brief Finds the raw physical vector index of a character by its CharId.
     */
    int findPhysicalIndex(const CharId& id) const {
        for (size_t i = 0; i < elements_.size(); ++i) {
            if (elements_[i].id == id) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    /**
     * @brief Clears all characters from the sequence.
     */
    void clear() {
        elements_.clear();
    }

    /**
     * @brief Directly sets the character sequence (used when restoring snapshots).
     */
    void loadCharacters(const std::vector<Character>& chars) {
        elements_ = chars;
    }

private:
    std::vector<Character> elements_;
};

} // namespace collabpad
