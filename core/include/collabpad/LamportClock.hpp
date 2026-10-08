#pragma once
#include <algorithm>
#include <cstdint>

namespace collabpad {
class LamportClock {
public:
    explicit LamportClock(uint64_t initialTime = 0)
        : time_(initialTime) {}
    uint64_t tick() {
        return ++time_;
    }
    void update(uint64_t receivedTime) {
        time_ = std::max(time_, receivedTime);
    }
    uint64_t getTime() const {
        return time_;
    }

private:
    uint64_t time_{0};
};

} 
