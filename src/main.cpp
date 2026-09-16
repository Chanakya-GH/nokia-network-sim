// netsim: a small multi-threaded network packet simulator.
//
// Topology built here:
//
//   Ingress -> Switch1 -> Switch2 -> Egress
//
// Each node runs on its own thread, connected by custom thread-safe
// bounded queues. The driver injects synthetic packets into Ingress,
// occasionally corrupting one on purpose so you can see the checksum-drop
// path exercised, then shuts the pipeline down cleanly and prints metrics.
//
// Run: ./netsim [num_packets] [queue_capacity]

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "netsim/node.hpp"
#include "netsim/queue_metrics.hpp"

using namespace netsim;
using namespace std::chrono_literals;

namespace {

Packet make_packet(std::uint64_t seq, bool corrupt = false) {
    std::vector<std::uint8_t> payload = {
        static_cast<std::uint8_t>(seq & 0xFF),
        static_cast<std::uint8_t>((seq >> 8) & 0xFF),
        0xDE, 0xAD, 0xBE, 0xEF,
    };
    Packet pkt(seq, payload);
    pkt.seal();
    if (corrupt) {
        // Deliberately break the sealed checksum to exercise the
        // checksum-drop path at a SwitchNode/EgressNode.
        auto corrupted_header = pkt.header();
        corrupted_header.checksum ^= 0xFFFFFFFF;
        // Packet has no public checksum setter by design (checksum is only
        // ever derived from payload via seal()); simplest way to simulate
        // corruption honestly is to mutate the payload after sealing
        // instead. Re-seal is intentionally NOT called here.
        (void)corrupted_header;
    }
    return pkt;
}

}  // namespace

int main(int argc, char** argv) {
    const int num_packets = (argc > 1) ? std::atoi(argv[1]) : 50;
    const std::size_t queue_capacity = (argc > 2) ? static_cast<std::size_t>(std::atoi(argv[2])) : 8;

    std::cout << "netsim starting: " << num_packets << " packets, queue capacity "
              << queue_capacity << "\n";

    Metrics metrics;

    IngressNode ingress("Ingress", queue_capacity, metrics);
    SwitchNode switch1("Switch1", queue_capacity, metrics, 200us);
    SwitchNode switch2("Switch2", queue_capacity, metrics, 200us);
    EgressNode egress("Egress", queue_capacity, metrics);

    ingress.connect_downstream(&switch1);
    switch1.connect_downstream(&switch2);
    switch2.connect_downstream(&egress);

    ingress.apply_link_event(LinkEvent::kStart);
    ingress.apply_link_event(LinkEvent::kInitDone);

    ingress.start();
    switch1.start();
    switch2.start();
    egress.start();

    for (int i = 0; i < num_packets; ++i) {
        // Every 10th packet is intentionally corrupted post-seal to
        // exercise the checksum-drop path.
        Packet pkt = make_packet(static_cast<std::uint64_t>(i));
        if (i % 10 == 9) {
            std::vector<std::uint8_t> tampered(pkt.payload());
            if (!tampered.empty()) {
                tampered[0] ^= 0xFF;
            }
            pkt = Packet(static_cast<std::uint64_t>(i), tampered);
            // Note: NOT calling seal() here -- this packet carries a
            // zeroed/default checksum, so verify() will correctly fail
            // at the first hop that checks it.
        }
        metrics.packets_sent++;
        if (!ingress.enqueue(std::move(pkt))) {
            metrics.packets_dropped_queue_full++;
        }
    }

    // Give the pipeline time to drain before shutting down. A more
    // sophisticated driver would wait on a "drained" condition instead of
    // sleeping; kept simple here and called out as a known simplification.
    std::this_thread::sleep_for(200ms);

    egress.stop();
    switch2.stop();
    switch1.stop();
    ingress.stop();

    std::cout << "netsim shutdown complete.\n";
    metrics.print(std::cout);

    return 0;
}
