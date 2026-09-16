#pragma once

#include <stdexcept>
#include <string>

namespace netsim {

// Models the lifecycle of a link between two nodes. Real optical/network
// links go through analogous states during bring-up, steady operation,
// degraded performance (rising error rate / partial signal loss), and
// failure -- this FSM captures that shape without modeling real physical
// layer signaling.
enum class LinkState {
    kDown,          // No connectivity.
    kInitializing,  // Bring-up in progress (e.g. simulated handshake delay).
    kUp,            // Fully operational.
    kDegraded,      // Operational but above the error-rate threshold.
};

enum class LinkEvent {
    kStart,             // Begin bring-up: Down -> Initializing.
    kInitDone,          // Bring-up complete: Initializing -> Up.
    kErrorThresholdHit, // Error rate crossed threshold: Up -> Degraded.
    kErrorsCleared,     // Error rate back to normal: Degraded -> Up.
    kFailure,           // Hard failure from any state: * -> Down.
};

const char* to_string(LinkState state);
const char* to_string(LinkEvent event);

// Thrown when an event is not a valid transition from the current state.
// Kept as a distinct type (rather than reusing std::logic_error directly)
// so callers/tests can catch it specifically.
class InvalidTransition : public std::logic_error {
public:
    InvalidTransition(LinkState from, LinkEvent event);
};

// A small, explicit state machine -- deliberately table-free/if-else based
// so the transition logic is easy to read and to unit test exhaustively.
// Not thread-safe by itself; callers (Node/Link owners) are responsible for
// synchronizing access, same as they do for the packet queues.
class LinkStateMachine {
public:
    LinkStateMachine() = default;

    LinkState state() const { return state_; }

    // Applies the event. Throws InvalidTransition if the event is not legal
    // from the current state.
    void apply(LinkEvent event);

private:
    LinkState state_ = LinkState::kDown;
};

}  // namespace netsim
