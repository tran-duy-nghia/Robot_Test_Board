#include "i2c_test.hpp"

#include "esp_err.h"

I2CTest::I2CTest(const Config &config)
    : m_config(config) {
}

I2CTest::~I2CTest() {
    if (m_busHandle != nullptr) {
        i2c_del_master_bus(m_busHandle);
        m_busHandle = nullptr;
    }
}

const char *I2CTest::id() const {
    return "I2C";
}

const char *I2CTest::name() const {
    return "I2C Bus Scan Test";
}

bool I2CTest::configureI2C() {
    if (!GPIO_IS_VALID_GPIO(m_config.sda_pin) ||
        !GPIO_IS_VALID_GPIO(m_config.scl_pin) ||
        m_config.sda_pin == m_config.scl_pin ||
        m_config.frequency_hz == 0 ||
        m_config.probe_timeout_ms == 0) {

        m_result.setFailure(
            TestError::INVALID_ARGUMENT,
            "Invalid I2C configuration");

        return false;
    }

    i2c_master_bus_config_t busConfig{};

    busConfig.i2c_port = m_config.port;
    busConfig.sda_io_num = m_config.sda_pin;
    busConfig.scl_io_num = m_config.scl_pin;

    busConfig.clk_source = I2C_CLK_SRC_DEFAULT;

    busConfig.glitch_ignore_cnt = 7;

    busConfig.flags.enable_internal_pullup =
        m_config.enable_internal_pullup;

    const esp_err_t err = i2c_new_master_bus(
        &busConfig,
        &m_busHandle);

    if (err != ESP_OK) {
        m_result.setFailure(
            TestError::HARDWARE_ERROR,
            "I2C master bus initialization failed");

        return false;
    }

    m_initialized = true;

    return true;
}

bool I2CTest::start() {
    if (m_state == TestState::RUNNING) {
        return false;
    }

    m_result.reset();

    m_completedCycles = 0;

    m_nextAddress = 0x08;

    m_foundDevices = 0;

    if (m_config.interval_ms == 0 ||
        m_config.probe_timeout_ms == 0) {

        m_result.setFailure(
            TestError::INVALID_ARGUMENT,
            "Invalid I2C test configuration");

        m_state = TestState::FAILED;

        return false;
    }

    if (!configureI2C()) {
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

void I2CTest::stop() {
    if (m_state != TestState::RUNNING) {
        return;
    }

    m_result.setFailure(
        TestError::INTERNAL_ERROR,
        "I2C test stopped by host");

    finish(TestState::STOPPED);
}

void I2CTest::process() {
    if (m_state != TestState::RUNNING) {
        return;
    }

    const TickType_t now = xTaskGetTickCount();

    if ((now - m_lastProcessTick) < m_intervalTicks) {
        return;
    }

    m_lastProcessTick = now;

    constexpr uint8_t FIRST_ADDRESS = 0x08;
    constexpr uint8_t LAST_ADDRESS = 0x77;

    const esp_err_t err = i2c_master_probe(
        m_busHandle,
        m_nextAddress,
        m_config.probe_timeout_ms);

    if (err == ESP_OK) {
        ++m_foundDevices;
    } else if (err != ESP_ERR_NOT_FOUND) {
        m_result.setFailure(
            TestError::HARDWARE_ERROR,
            "I2C device probe failed");

        finish(TestState::FAILED);

        return;
    }

    ++m_nextAddress;

    if (m_nextAddress > LAST_ADDRESS) {
        if (m_foundDevices > 0) {
            m_result.setSuccess(
                "I2C bus scan passed");

            finish(TestState::PASSED);
        } else {
            m_result.setFailure(
                TestError::VALIDATION_FAILED,
                "No I2C devices found");

            finish(TestState::FAILED);
        }
    }
}

void I2CTest::finish(TestState state) {
    m_state = state;

    if (m_busHandle != nullptr) {
        i2c_del_master_bus(m_busHandle);
        m_busHandle = nullptr;
    }

    m_initialized = false;
}

TestState I2CTest::state() const {
    return m_state;
}

const TestResult &I2CTest::result() const {
    return m_result;
}