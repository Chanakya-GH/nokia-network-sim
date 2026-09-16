#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "netsim/bounded_queue.hpp"
#include "netsim/link_state.hpp"
#include "netsim/packet.hpp"
#include "netsim/queue_metrics.hpp"

namespace netsim {

// Abstract base for every simulated network element. Each concrete Node
// owns its inbound queue and runs its processing loop on a dedicated
// std::thread (thread-per-node model -- see README "Design Decisions" for
// the tradeoff discussion vs. a shared thread pool).
//
// Lifecycle: construct -> start() -> ... simulation runs ... -> stop() ->
// join() happens inside stop(). Subclasses implement run_once(), which is
// called in a loop until the node is told to stop or its inbound queue is
// closed and drained.
class Node {
public:
    Node(std::string name, std::size_t inbound_capacity, Metrics& metrics);
    virtual ~Node();

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    const std::string& name() const { return name_; }

    // Connects this node's output to a downstream node's inbound queue.
    // Kept simple (single downstream) for this simulator; a real topology
    // class could generalize this to multiple downstream links.
    void connect_downstream(Node* downstream);

    // Starts the processing thread.
    void start();

    // Signals shutdown (closes the inbound queue, sets running_ = false),
    // then joins the thread. Safe to call once; idempotent if called again.
    void stop();

    // Called by an upstream node (or the driver, for ingress nodes) to
    // hand a packet to this node. Blocking push -- applies backpressure.
    bool enqueue(Packet packet);

    LinkState link_state() const { return link_fsm_.state(); }
    void apply_link_event(LinkEvent event) { link_fsm_.apply(event); }

protected:
    // Subclasses implement per-packet behavior: inspect/modify/forward.
    // Returning false tells the loop to drop the packet (e.g. checksum
    // failure); returning true means it was handled (typically forwarded
    // downstream by the override itself).
    virtual bool process(Packet& packet) = 0;

    void forward_downstream(Packet packet);

    BoundedQueue<Packet> inbound_;
    Metrics& metrics_;

private:
    void run_loop();

    std::string name_;
    Node* downstream_ = nullptr;
    LinkStateMachine link_fsm_;
    std::thread worker_;
    std::atomic<bool> running_{false};
};

// Entry point of the simulated topology: has no real upstream, packets are
// injected directly by the driver (main.cpp) via enqueue().
class IngressNode : public Node {
public:
    using Node::Node;

protected:
    bool process(Packet& packet) override;
};

// Interior node: verifies the packet's checksum (simulating per-hop
// integrity checking), drops corrupted packets, otherwise forwards.
class SwitchNode : public Node {
public:
    SwitchNode(std::string name, std::size_t inbound_capacity, Metrics& metrics,
               std::chrono::microseconds simulated_latency);

protected:
    bool process(Packet& packet) override;

private:
    std::chrono::microseconds simulated_latency_;
};

// Terminal node: records delivery in the metrics, does not forward further.
class EgressNode : public Node {
public:
    using Node::Node;

protected:
    bool process(Packet& packet) override;
};

}  // namespace netsim
