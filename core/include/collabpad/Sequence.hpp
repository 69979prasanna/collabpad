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

    Character() = default;

    Character(CharId charId, char val)
        : id(charId), afterId(CharId::root()), value(val) {}

    Character(CharId charId, CharId parentId, char val)
        : id(charId), afterId(parentId), value(val) {}
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

    std::string getText() const {
        std::string text;
        text.reserve(elements_.size());
        for (const auto& ch : elements_) {
            text.push_back(ch.value);
        }
        return text;
    }

    size_t size() const {
        return elements_.size();
    }

    bool empty() const {
        return elements_.empty();
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
    bool insertAfter(const CharId& id, const CharId& afterId, char value) {
        if (contains(id)) {
            return false;
        }
        size_t index = findInsertIndex(id, afterId);
        elements_.insert(elements_.begin() + index, Character{id, afterId, value});
        return true;
    }

    /**
     * @brief Gets the CharId of the character at the specified 0-based visual index.
     */
    CharId getIdAt(size_t index) const {
        if (index < elements_.size()) {
            return elements_[index].id;
        }
        return CharId::root();
    }

    /**
     * @brief Finds the visual index of a character by its CharId.
     */
    int findIndex(const CharId& id) const {
        for (size_t i = 0; i < elements_.size(); ++i) {
            if (elements_[i].id == id) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

private:
    std::vector<Character> elements_;
};

} // namespace collabpad
