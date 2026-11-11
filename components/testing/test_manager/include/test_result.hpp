#pragma once

#include <cstdint>
#include <cstring>

constexpr std::size_t TEST_MESSAGE_MAX_LENGTH = 128;

enum class TestError : uint16_t {
    NONE = 0,

    INVALID_ARGUMENT,
    RESOURCE_BUSY,
    HARDWARE_ERROR,
    COMMUNICATION_ERROR,
    TIMEOUT,
    VALIDATION_FAILED,
    INTERNAL_ERROR
};

struct TestResult {
    bool success = false;

    TestError errorCode = TestError::NONE;

    char message[TEST_MESSAGE_MAX_LENGTH] = {};

    void setSuccess(const char* msg = nullptr) {
        success = true;
        errorCode = TestError::NONE;
        setMessage(msg);
    }

    void setFailure(TestError error, const char* msg = nullptr) {
        success = false;
        errorCode = error;
        setMessage(msg);
    }

    void setMessage(const char* msg) {
        if (msg == nullptr) {
            message[0] = '\0';
            return;
        }

        std::strncpy(message, msg, TEST_MESSAGE_MAX_LENGTH - 1);
        message[TEST_MESSAGE_MAX_LENGTH - 1] = '\0';
    }

    void reset() {
        success = false;
        errorCode = TestError::NONE;
        message[0] = '\0';
    }
};

constexpr const char* testErrorToString(TestError error) {
    switch (error) {
        case TestError::NONE:
            return "NONE";

        case TestError::INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";

        case TestError::RESOURCE_BUSY:
            return "RESOURCE_BUSY";

        case TestError::HARDWARE_ERROR:
            return "HARDWARE_ERROR";

        case TestError::COMMUNICATION_ERROR:
            return "COMMUNICATION_ERROR";

        case TestError::TIMEOUT:
            return "TIMEOUT";

        case TestError::VALIDATION_FAILED:
            return "VALIDATION_FAILED";

        case TestError::INTERNAL_ERROR:
            return "INTERNAL_ERROR";

        default:
            return "UNKNOWN";
    }
}
