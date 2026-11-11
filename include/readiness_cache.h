#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>

template<class T> class ReadinessCache {
    mutable std::mutex mutex_;
    T value_{};
    bool valid_ = false;
    uint64_t published_ = 0;
    std::atomic<bool> requested_{true};
public:
    void request_refresh() { requested_ = true; }
    T snapshot() const { std::lock_guard<std::mutex> lock(mutex_); return value_; }
    template<class Scan, class Canceled>
    bool refresh(uint64_t now, uint64_t minimum, uint64_t periodic, Scan scan, Canceled canceled) {
        if (canceled()) return false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (valid_ && now - published_ < minimum) return false;
            if (valid_ && !requested_.load() && now - published_ < periodic) return false;
            requested_ = false;
        }
        // Never hold the snapshot lock while performing filesystem I/O.
        auto next = scan();
        if (canceled()) { requested_ = true; return false; }
        std::lock_guard<std::mutex> lock(mutex_);
        value_ = next;
        valid_ = true;
        published_ = now;
        return true;
    }
};
