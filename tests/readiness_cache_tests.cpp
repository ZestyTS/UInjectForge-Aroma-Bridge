#include "readiness_cache.h"
#include <cassert>
#include <future>
#include <chrono>
int main() {
    ReadinessCache<int> cache;
    int scans = 0;
    auto scan = [&] { return ++scans; };
    auto running = [] { return false; };
    assert(cache.refresh(1, 10, 100, scan, running));
    for (int i=0; i<100; ++i) cache.request_refresh();
    assert(!cache.refresh(5, 10, 100, scan, running));
    assert(cache.refresh(11, 10, 100, scan, running));
    assert(scans == 2);
    std::promise<void> started, finish;
    auto done = finish.get_future();
    cache.request_refresh();
    auto worker = std::async(std::launch::async, [&] {
        return cache.refresh(21, 10, 100, [&] { started.set_value(); done.wait(); return 3; }, running);
    });
    started.get_future().wait();
    auto reader = std::async(std::launch::async, [&] { return cache.snapshot(); });
    assert(reader.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    assert(reader.get() == 2);
    finish.set_value();
    assert(worker.get());
    bool canceled = false;
    cache.request_refresh();
    assert(!cache.refresh(31, 10, 100, [&] { canceled = true; return 4; }, [&] { return canceled; }));
    assert(cache.snapshot() == 3);
}
