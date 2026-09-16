#include "netsim/packet.hpp"

#include <gtest/gtest.h>

using namespace netsim;

TEST(Packet, SealedPacketVerifiesSuccessfully) {
    Packet pkt(1, {0x01, 0x02, 0x03, 0x04});
    pkt.seal();
    EXPECT_TRUE(pkt.verify());
}

TEST(Packet, UnsealedPacketFailsVerification) {
    // Default-constructed header has checksum == 0, which will not match
    // the computed checksum of a non-empty payload.
    Packet pkt(1, {0x01, 0x02, 0x03, 0x04});
    EXPECT_FALSE(pkt.verify());
}

TEST(Packet, TamperedPayloadAfterSealFailsVerification) {
    Packet pkt(1, {0x01, 0x02, 0x03, 0x04});
    pkt.seal();
    ASSERT_TRUE(pkt.verify());

    // Simulate corruption in transit: construct a new packet with the same
    // sequence number but different payload, without resealing. (Packet
    // has no payload mutator by design -- corruption is modeled by
    // constructing a fresh packet and deliberately not calling seal(), the
    // same technique main.cpp uses.)
    Packet corrupted(1, {0xFF, 0x02, 0x03, 0x04});
    EXPECT_FALSE(corrupted.verify());
}

TEST(Packet, EmptyPayloadSealsAndVerifies) {
    Packet pkt(1, {});
    pkt.seal();
    EXPECT_TRUE(pkt.verify());
}

TEST(Packet, HopCountStartsAtZeroAndIncrements) {
    Packet pkt(1, {0x01});
    EXPECT_EQ(pkt.hop_count(), 0);
    pkt.increment_hop_count();
    pkt.increment_hop_count();
    EXPECT_EQ(pkt.hop_count(), 2);
}

TEST(Packet, SequenceNumberIsPreserved) {
    Packet pkt(12345, {0x01});
    EXPECT_EQ(pkt.header().sequence_number, 12345u);
}

TEST(Packet, DifferentPayloadsProduceDifferentChecksums) {
    Packet a(1, {0x01, 0x02, 0x03});
    Packet b(1, {0x03, 0x02, 0x01});
    a.seal();
    b.seal();
    EXPECT_NE(a.header().checksum, b.header().checksum);
}
