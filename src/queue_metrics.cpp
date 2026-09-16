#include "netsim/queue_metrics.hpp"

namespace netsim {

void Metrics::print(std::ostream& os) const {
    const auto sent = packets_sent.load();
    const auto delivered = packets_delivered.load();
    const auto drop_full = packets_dropped_queue_full.load();
    const auto drop_checksum = packets_dropped_checksum.load();
    const auto total_dropped = drop_full + drop_checksum;

    os << "--- Simulation Metrics ---\n"
       << "  Packets sent:                " << sent << "\n"
       << "  Packets delivered:            " << delivered << "\n"
       << "  Packets dropped (queue full): " << drop_full << "\n"
       << "  Packets dropped (checksum):   " << drop_checksum << "\n"
       << "  Total dropped:                " << total_dropped << "\n";

    if (sent > 0) {
        const double delivery_rate = 100.0 * static_cast<double>(delivered) / static_cast<double>(sent);
        os << "  Delivery rate:                " << delivery_rate << "%\n";
    }
}

}  // namespace netsim
