#include "gpio_test.hpp"

#include "esp_err.h"

GpioTest::GpioTest(const Config& config)
    : m_config(config) {
}

const char* GpioTest::id() const {
    return "GPIO";
}

const char* GpioTest::name() const {
    return "GPIO Output Readback Test";
}

bool GpioTest::configurePin() {
    if (!GPIO_IS_VALID_OUTPUT_GPIO(m_config.pin)) {
        m_result.setFailure(
            TestError::INVALID_ARGUMENT,
            "Invalid GPIO output pin"
        );

        return false;
    }

    gpio_config_t config{};

    config.pin_bit_mask = 1ULL << static_cast<uint32_t>(m_config.pin);

    config.mode = GPIO_MODE_OUTPUT;

    config.pull_up_en = GPIO_PULLUP_DISABLE;

    config.pull_down_en = GPIO_PULLDOWN_DISABLE;

    config.intr_type = GPIO_INTR_DISABLE;

    const esp_err_t err = gpio_config(&config);

    if (err != ESP_OK) {
        m_result.setFailure(
            TestError::HARDWARE_ERROR,
            "GPIO configuration failed"
        );

        return false;
    }

    m_initialized = true;

    return true;
}

bool GpioTest::start() {
    if (m_state == TestState::RUNNING) {
        return false;
    }

    m_result.reset();

    m_completedCycles = 0;

    m_expectedLevel = false;

    if (m_config.cycles == 0 || m_config.interval_ms == 0) {
        m_result.setFailure(
            TestError::INVALID_ARGUMENT,
            "Invalid GPIO test configuration"
        );

        m_state = TestState::FAILED;

        return false;
    }

    if (!configurePin()) {
        m_state = TestState::FAILED;

        return false;
    }

    const esp_err_t err = gpio_set_level(m_config.pin, 0);

    if (err != ESP_OK) {
        m_result.setFailure(
            TestError::HARDWARE_ERROR,
            "Failed to initialize GPIO LOW"
        );

        m_state = TestState::FAILED;

        return false;
    }

    m_intervalTicks = pdMS_TO_TICKS(m_config.interval_ms);

    if (m_intervalTicks == 0) {
        m_intervalTicks = 1;
    }

    m_lastProcessTick = xTaskGetTickCount();

    m_state = TestState::RUNNING;

    return true;
}

void GpioTest::stop() {
    if (m_state != TestState::RUNNING) {
        return;
    }

    if (m_initialized) {
        gpio_set_level(m_config.pin, 0);
    }

    m_result.setFailure(
        TestError::INTERNAL_ERROR,
        "GPIO test stopped by host"
    );

    m_state = TestState::STOPPED;
}

bool GpioTest::verifyLevel(const int expectedLevel) {
    const int actualLevel = gpio_get_level(m_config.pin);

    if (actualLevel < 0 || actualLevel != expectedLevel) {
        m_result.setFailure(
            TestError::VALIDATION_FAILED,
            "GPIO output readback mismatch"
        );

        finish(TestState::FAILED);

        return false;
    }

    return true;
}

void GpioTest::process() {
    if (m_state != TestState::RUNNING) {
        return;
    }

    const TickType_t now = xTaskGetTickCount();

    if ((now - m_lastProcessTick) < m_intervalTicks) {
        return;
    }

    m_lastProcessTick = now;

    // Alternate GPIO output between HIGH and LOW.
    m_expectedLevel = !m_expectedLevel;

    const int level = m_expectedLevel ? 1 : 0;

    const esp_err_t err = gpio_set_level(m_config.pin, level);

    if (err != ESP_OK) {
        m_result.setFailure(
            TestError::HARDWARE_ERROR,
            "GPIO write failed"
        );

        finish(TestState::FAILED);

        return;
    }

    if (!verifyLevel(level)) {
        return;
    }

    // One complete cycle consists of HIGH and LOW.
    if (!m_expectedLevel) {
        ++m_completedCycles;
    }

    if (m_completedCycles >= m_config.cycles) {
        m_result.setSuccess(
            "GPIO output readback passed"
        );

        finish(TestState::PASSED);
    }
}

void GpioTest::finish(const TestState state) {
    m_state = state;

    if (m_initialized) {
        gpio_set_level(m_config.pin, 0);
    }
}

TestState GpioTest::state() const {
    return m_state;
}

const TestResult& GpioTest::result() const {
    return m_result;
}