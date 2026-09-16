#include "netsim/node.hpp"

#include <thread>

namespace netsim {

Node::Node(std::string name, std::size_t inbound_capacity, Metrics& metrics)
    : inbound_(inbound_capacity), metrics_(metrics), name_(std::move(name)) {}

Node::~Node() {
    stop();
}

void Node::connect_downstream(Node* downstream) {
    downstream_ = downstream;
}

void Node::start() {
    running_ = true;
    worker_ = std::thread(&Node::run_loop, this);
}

void Node::stop() {
    if (!running_.exchange(false)) {
        // Already stopped (or never started); still make sure the queue is
        // closed so any blocked callers wake up, but avoid double-join.
        inbound_.close();
        return;
    }
    inbound_.close();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool Node::enqueue(Packet packet) {
    return inbound_.push(std::move(packet));
}

void Node::forward_downstream(Packet packet) {
    if (downstream_ == nullptr) {
        return;
    }
    packet.increment_hop_count();
    if (!downstream_->enqueue(std::move(packet))) {
        // Downstream queue closed (shutting down) or, if we later switch
        // this call site to try_push, full. Counted as a drop either way.
        metrics_.packets_dropped_queue_full++;
    }
}

void Node::run_loop() {
    while (true) {
        std::optional<Packet> maybe_packet = inbound_.pop();
        if (!maybe_packet.has_value()) {
            // Queue closed and drained: this node's work is done.
            break;
        }
        Packet packet = std::move(*maybe_packet);
        process(packet);
    }
}

// --- IngressNode ---

bool IngressNode::process(Packet& packet) {
    forward_downstream(std::move(packet));
    return true;
}

// --- SwitchNode ---

SwitchNode::SwitchNode(std::string name, std::size_t inbound_capacity, Metrics& metrics,
                        std::chrono::microseconds simulated_latency)
    : Node(std::move(name), inbound_capacity, metrics), simulated_latency_(simulated_latency) {}

bool SwitchNode::process(Packet& packet) {
    if (simulated_latency_.count() > 0) {
        std::this_thread::sleep_for(simulated_latency_);
    }
    if (!packet.verify()) {
        metrics_.packets_dropped_checksum++;
        return false;
    }
    forward_downstream(std::move(packet));
    return true;
}

// --- EgressNode ---

bool EgressNode::process(Packet& packet) {
    if (!packet.verify()) {
        metrics_.packets_dropped_checksum++;
        return false;
    }
    metrics_.packets_delivered++;
    return true;
}

}  // namespace netsim
