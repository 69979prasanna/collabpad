#pragma once

#include "CharId.hpp"
#include <string>
#include <vector>

namespace collabpad {
struct Character {
    CharId id;
    char value{'\0'};
};
class Sequence {
public:
    Sequence() = default;
    void insert(size_t index, CharId id, char value) {
        if (index > elements_.size()) {
            index = elements_.size();
        }
        elements_.insert(elements_.begin() + index, Character{id, value});
    }
    bool contains(const CharId& id) const {
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

private:
    std::vector<Character> elements_;
};

} 
