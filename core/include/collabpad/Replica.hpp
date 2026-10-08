#pragma once

#include "CharId.hpp"
#include "LamportClock.hpp"
#include "Sequence.hpp"
#include <cstdint>
#include <string>

namespace collabpad {
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
    uint32_t getSiteId() const {
        return siteId_;
    }

    std::string getText() const {
        return sequence_.getText();
    }

    const Sequence& getSequence() const {
        return sequence_;
    }

    const LamportClock& getClock() const {
        return clock_;
    }

private:
    uint32_t siteId_;
    LamportClock clock_;
    Sequence sequence_;
};

} 
