#include "m288t_test.hpp"

#include <cstdio>

#include "esp_err.h"

M288TTest::M288TTest(const Config &config)
    : m_config(config),
      m_motor(config.motor) {
}

M288TTest::~M288TTest() {
    m_motor.deinit();
}

void M288TTest::configure(const Config &config) {
    if (m_state != TestState::RUNNING) {
        m_config = config;
        m_motor.configure(config.motor);
    }
}

const char *M288TTest::id() const {
    return "M288T";
}

const char *M288TTest::name() const {
    return "DYNAMIXEL XL330-M288-T Communication Test";
}

TestState M288TTest::state() const {
    return m_state;
}

const TestResult &M288TTest::result() const {
    return m_result;
}

bool M288TTest::start() {
    if (m_state == TestState::RUNNING) {
        return false;
    }

    m_motor.deinit();
    m_result.reset();
    m_state = TestState::RUNNING;
    m_initialized = false;

    esp_err_t err = m_motor.init();
    if (err != ESP_OK) {
        fail(
            TestError::HARDWARE_ERROR,
            "M288T UART/half-duplex transport init failed");
        return true;
    }

    m_initialized = true;

    M288T::DeviceInfo info{};
    err = m_motor.identify(info);

    if (err == ESP_ERR_TIMEOUT) {
        char message[96]{};
        std::snprintf(message, sizeof(message), "M288T timeout at %s", info.failed_step[0] ? info.failed_step : "UNKNOWN_STEP");
        fail(TestError::TIMEOUT, message);
        return true;
    }

    if (err == ESP_ERR_NOT_FOUND) {
        fail(TestError::VALIDATION_FAILED, "DYNAMIXEL found but Model Number is not XL330-M288-T (1200)");
        return true;
    }

    if (err == ESP_ERR_INVALID_RESPONSE) {
        fail(TestError::VALIDATION_FAILED, "Invalid DYNAMIXEL status packet or device information mismatch");
        return true;
    }

    if (err != ESP_OK) {
        fail(TestError::COMMUNICATION_ERROR, "DYNAMIXEL Protocol 2.0 transaction failed");
        return true;
    }

    char message[TEST_MESSAGE_MAX_LENGTH]{};
    std::snprintf(
        message,
        sizeof(message),
        "XL330-M288-T MODEL=%u FW=%u ID=%u BAUD=57600 HW_ERR=0x%02X",
        static_cast<unsigned>(info.model_number),
        static_cast<unsigned>(info.firmware_version),
        static_cast<unsigned>(info.id),
        static_cast<unsigned>(info.hardware_error_status));

    m_result.setSuccess(message);
    finish(TestState::PASSED);
    return true;
}

void M288TTest::stop() {
    if (m_state != TestState::RUNNING) {
        return;
    }

    m_motor.deinit();
    m_result.setFailure(TestError::INTERNAL_ERROR, "M288T test stopped by host");
    finish(TestState::STOPPED);
}

void M288TTest::process() {
    // The communication/identification sequence is intentionally synchronous.
    // TestManager accepts terminal states returned by start().
}

void M288TTest::finish(TestState finalState) {
    m_motor.deinit();
    m_state = finalState;
    m_initialized = false;
}

void M288TTest::fail(TestError error, const char *message) {
    m_result.setFailure(error, message);
    finish(TestState::FAILED);
}
