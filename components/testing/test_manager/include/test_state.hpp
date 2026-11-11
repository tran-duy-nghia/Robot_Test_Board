#pragma once

#include <cstdint>

enum class TestState : uint8_t {
    IDLE = 0,
    RUNNING,
    PASSED,
    FAILED,
    STOPPED
};

constexpr const char* testStateToString(TestState state) {
    switch (state) {
        case TestState::IDLE:
            return "IDLE";

        case TestState::RUNNING:
            return "RUNNING";

        case TestState::PASSED:
            return "PASSED";

        case TestState::FAILED:
            return "FAILED";

        case TestState::STOPPED:
            return "STOPPED";

        default:
            return "UNKNOWN";
    }
}

constexpr bool isTestFinished(TestState state) {
    return state == TestState::PASSED ||
           state == TestState::FAILED ||
           state == TestState::STOPPED;
}