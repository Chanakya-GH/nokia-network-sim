#include "netsim/link_state.hpp"

#include <gtest/gtest.h>

using namespace netsim;

TEST(LinkStateMachine, StartsDown) {
    LinkStateMachine fsm;
    EXPECT_EQ(fsm.state(), LinkState::kDown);
}

TEST(LinkStateMachine, HappyPathBringUp) {
    LinkStateMachine fsm;
    fsm.apply(LinkEvent::kStart);
    EXPECT_EQ(fsm.state(), LinkState::kInitializing);
    fsm.apply(LinkEvent::kInitDone);
    EXPECT_EQ(fsm.state(), LinkState::kUp);
}

TEST(LinkStateMachine, DegradationAndRecovery) {
    LinkStateMachine fsm;
    fsm.apply(LinkEvent::kStart);
    fsm.apply(LinkEvent::kInitDone);
    fsm.apply(LinkEvent::kErrorThresholdHit);
    EXPECT_EQ(fsm.state(), LinkState::kDegraded);
    fsm.apply(LinkEvent::kErrorsCleared);
    EXPECT_EQ(fsm.state(), LinkState::kUp);
}

TEST(LinkStateMachine, FailureFromUpGoesToDown) {
    LinkStateMachine fsm;
    fsm.apply(LinkEvent::kStart);
    fsm.apply(LinkEvent::kInitDone);
    fsm.apply(LinkEvent::kFailure);
    EXPECT_EQ(fsm.state(), LinkState::kDown);
}

TEST(LinkStateMachine, FailureFromDegradedGoesToDown) {
    LinkStateMachine fsm;
    fsm.apply(LinkEvent::kStart);
    fsm.apply(LinkEvent::kInitDone);
    fsm.apply(LinkEvent::kErrorThresholdHit);
    fsm.apply(LinkEvent::kFailure);
    EXPECT_EQ(fsm.state(), LinkState::kDown);
}

TEST(LinkStateMachine, FailureFromInitializingGoesToDown) {
    LinkStateMachine fsm;
    fsm.apply(LinkEvent::kStart);
    fsm.apply(LinkEvent::kFailure);
    EXPECT_EQ(fsm.state(), LinkState::kDown);
}

TEST(LinkStateMachine, InvalidTransitionsThrow) {
    LinkStateMachine fsm;
    // Can't InitDone before Start.
    EXPECT_THROW(fsm.apply(LinkEvent::kInitDone), InvalidTransition);
    // Can't hit error threshold while Down.
    EXPECT_THROW(fsm.apply(LinkEvent::kErrorThresholdHit), InvalidTransition);
    // Can't fail something that's already Down.
    EXPECT_THROW(fsm.apply(LinkEvent::kFailure), InvalidTransition);
}

TEST(LinkStateMachine, DoubleStartIsInvalid) {
    LinkStateMachine fsm;
    fsm.apply(LinkEvent::kStart);
    EXPECT_THROW(fsm.apply(LinkEvent::kStart), InvalidTransition);
}

TEST(LinkStateMachine, ErrorsClearedOnlyValidFromDegraded) {
    LinkStateMachine fsm;
    fsm.apply(LinkEvent::kStart);
    fsm.apply(LinkEvent::kInitDone);
    // Up -> ErrorsCleared is not a defined transition.
    EXPECT_THROW(fsm.apply(LinkEvent::kErrorsCleared), InvalidTransition);
}

TEST(ToString, CoversAllStatesAndEvents) {
    EXPECT_STREQ(to_string(LinkState::kDown), "DOWN");
    EXPECT_STREQ(to_string(LinkState::kInitializing), "INITIALIZING");
    EXPECT_STREQ(to_string(LinkState::kUp), "UP");
    EXPECT_STREQ(to_string(LinkState::kDegraded), "DEGRADED");

    EXPECT_STREQ(to_string(LinkEvent::kStart), "START");
    EXPECT_STREQ(to_string(LinkEvent::kInitDone), "INIT_DONE");
    EXPECT_STREQ(to_string(LinkEvent::kErrorThresholdHit), "ERROR_THRESHOLD_HIT");
    EXPECT_STREQ(to_string(LinkEvent::kErrorsCleared), "ERRORS_CLEARED");
    EXPECT_STREQ(to_string(LinkEvent::kFailure), "FAILURE");
}
