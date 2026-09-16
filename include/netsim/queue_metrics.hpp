#pragma once

#include <atomic>
#include <cstdint>
#include <ostream>

namespace netsim {

// Process-wide simulation counters. All fields are atomics so any node
// thread can update them without a separate lock -- deliberately simpler
// than routing every metric update through the bounded queue's mutex,
// and a natural discussion point about when atomics are sufficient vs.
// when you need a full mutex (answer: single independent counters, yes;
// multi-field invariants that must update together, no).
struct Metrics {
    std::atomic<std::uint64_t> packets_sent{0};
    std::atomic<std::uint64_t> packets_delivered{0};
    std::atomic<std::uint64_t> packets_dropped_queue_full{0};
    std::atomic<std::uint64_t> packets_dropped_checksum{0};

    void print(std::ostream& os) const;
};

}  // namespace netsim
