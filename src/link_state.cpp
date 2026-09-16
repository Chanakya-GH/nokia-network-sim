#include "netsim/link_state.hpp"

namespace netsim {

const char* to_string(LinkState state) {
    switch (state) {
        case LinkState::kDown: return "DOWN";
        case LinkState::kInitializing: return "INITIALIZING";
        case LinkState::kUp: return "UP";
        case LinkState::kDegraded: return "DEGRADED";
    }
    return "UNKNOWN";
}

const char* to_string(LinkEvent event) {
    switch (event) {
        case LinkEvent::kStart: return "START";
        case LinkEvent::kInitDone: return "INIT_DONE";
        case LinkEvent::kErrorThresholdHit: return "ERROR_THRESHOLD_HIT";
        case LinkEvent::kErrorsCleared: return "ERRORS_CLEARED";
        case LinkEvent::kFailure: return "FAILURE";
    }
    return "UNKNOWN";
}

InvalidTransition::InvalidTransition(LinkState from, LinkEvent event)
    : std::logic_error(std::string("invalid transition: event ") + to_string(event) +
                        " from state " + to_string(from)) {}

void LinkStateMachine::apply(LinkEvent event) {
    // kFailure is legal from any state except Down (nothing to fail from).
    if (event == LinkEvent::kFailure) {
        if (state_ == LinkState::kDown) {
            throw InvalidTransition(state_, event);
        }
        state_ = LinkState::kDown;
        return;
    }

    switch (state_) {
        case LinkState::kDown:
            if (event == LinkEvent::kStart) {
                state_ = LinkState::kInitializing;
                return;
            }
            break;
        case LinkState::kInitializing:
            if (event == LinkEvent::kInitDone) {
                state_ = LinkState::kUp;
                return;
            }
            break;
        case LinkState::kUp:
            if (event == LinkEvent::kErrorThresholdHit) {
                state_ = LinkState::kDegraded;
                return;
            }
            break;
        case LinkState::kDegraded:
            if (event == LinkEvent::kErrorsCleared) {
                state_ = LinkState::kUp;
                return;
            }
            break;
    }
    throw InvalidTransition(state_, event);
}

}  // namespace netsim
