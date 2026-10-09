#pragma once

#include <cstdint>

namespace collabpad {
struct CharId {
    uint32_t siteId{0};
    uint64_t clock{0};

    CharId() = default;
    CharId(uint32_t site, uint64_t clk)
        : siteId(site), clock(clk) {}

    static CharId root() {
        return CharId{0, 0};
    }

    bool isRoot() const {
        return siteId == 0 && clock == 0;
    }

    bool operator==(const CharId& other) const {
        return siteId == other.siteId && clock == other.clock;
    }

    bool operator!=(const CharId& other) const {
        return !(*this == other);
    }

    bool operator<(const CharId& other) const {
        if (clock != other.clock) {
            return clock < other.clock;
        }
        return siteId < other.siteId;
    }

    bool operator>(const CharId& other) const {
        return other < *this;
    }

    bool operator<=(const CharId& other) const {
        return !(other < *this);
    }

    bool operator>=(const CharId& other) const {
        return !(*this < other);
    }
};

} // namespace collabpad
