#include "netsim/packet.hpp"

namespace netsim {

Packet::Packet(std::uint64_t sequence_number, std::vector<std::uint8_t> payload)
    : payload_(std::move(payload)) {
    header_.sequence_number = sequence_number;
}

std::uint32_t Packet::compute_checksum(const std::vector<std::uint8_t>& payload) {
    // Intentionally simple (Fletcher-32-ish rolling sum), NOT cryptographic.
    // Good enough to detect the kind of corruption this simulation injects,
    // and cheap to compute per-hop the way a real line-rate device would
    // need a cheap check.
    std::uint32_t sum1 = 0;
    std::uint32_t sum2 = 0;
    for (std::uint8_t byte : payload) {
        sum1 = (sum1 + byte) % 65521;
        sum2 = (sum2 + sum1) % 65521;
    }
    return (sum2 << 16) | sum1;
}

void Packet::seal() {
    header_.sync = FrameHeader::kSyncPattern;
    header_.checksum = compute_checksum(payload_);
}

bool Packet::verify() const {
    if (header_.sync != FrameHeader::kSyncPattern) {
        return false;
    }
    return header_.checksum == compute_checksum(payload_);
}

}  // namespace netsim
