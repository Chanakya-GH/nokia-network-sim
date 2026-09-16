#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace netsim {

// Lightweight framing header, loosely inspired by OTN/SONET-style framing:
// a sync pattern, a monotonic sequence number, and a checksum over the
// payload for basic corruption detection. This is NOT a real optical
// framing protocol implementation -- it exists to give the simulation
// something concrete to validate/drop packets on, and to give an honest,
// scoped talking point about framing/error-detection concepts.
struct FrameHeader {
    static constexpr std::uint16_t kSyncPattern = 0xF6F6;

    std::uint16_t sync = kSyncPattern;
    std::uint64_t sequence_number = 0;
    std::uint32_t checksum = 0;
};

class Packet {
public:
    Packet() = default;
    Packet(std::uint64_t sequence_number, std::vector<std::uint8_t> payload);

    const FrameHeader& header() const { return header_; }
    const std::vector<std::uint8_t>& payload() const { return payload_; }

    // Recomputes the checksum from the current payload and stores it in
    // the header. Call this once, at creation ("transmission").
    void seal();

    // Returns true if the stored checksum matches a freshly computed one
    // over the current payload. Used by nodes to simulate corruption
    // detection / drop-on-error behavior.
    bool verify() const;

    // Simple hop counter, incremented by each node the packet passes
    // through -- useful for detecting routing loops and for metrics.
    void increment_hop_count() { hop_count_++; }
    int hop_count() const { return hop_count_; }

private:
    static std::uint32_t compute_checksum(const std::vector<std::uint8_t>& payload);

    FrameHeader header_;
    std::vector<std::uint8_t> payload_;
    int hop_count_ = 0;
};

}  // namespace netsim
