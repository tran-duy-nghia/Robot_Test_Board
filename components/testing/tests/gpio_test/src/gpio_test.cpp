#include "gpio_test.hpp"

#include "esp_err.h"
#include "esp_intr_alloc.h"

GpioTest::GpioTest(const Config &config) : m_config(config) {}

GpioTest::~GpioTest() {
    if (m_state == TestState::RUNNING) {
        stop();
    }

    cleanUp();
}

const char *GpioTest::id() const { return "GPIO"; }

const char *GpioTest::name() const { return "GPIO Output Readback Test"; }

bool GpioTest::configureInput() {
    if (!GPIO_IS_VALID_GPIO(m_config.pin)) {
        m_result.setFailure(TestError::INVALID_ARGUMENT, "Invalid GPIO input pin");

        return false;
    }

    if (m_config.pull_up && m_config.pull_down) {
        m_result.setFailure(TestError::INVALID_ARGUMENT, "Pull-up and pull-down cannot be enabled simultaneously");

        return false;
    }

    if (m_config.interrupt_type == GPIO_INTR_DISABLE) {
        m_result.setFailure(TestError::INVALID_ARGUMENT, "Interrupt type must be specified for input mode");

        return false;
    }

    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << static_cast<uint32_t>(m_config.pin);
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = m_config.pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    config.pull_down_en = m_config.pull_down ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    config.intr_type = m_config.interrupt_type;

    const esp_err_t err = gpio_config(&config);

    if (err != ESP_OK) {
        m_result.setFailure(TestError::HARDWARE_ERROR, "GPIO configuration failed");

        return false;
    }

    m_initialized = true;

    return true;
}

bool GpioTest::configureOutput() {
    if (!GPIO_IS_VALID_OUTPUT_GPIO(m_config.pin)) {
        m_result.setFailure(TestError::INVALID_ARGUMENT, "Invalid GPIO output pin");

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
        m_result.setFailure(TestError::HARDWARE_ERROR, "GPIO configuration failed");

        return false;
    }

    m_initialized = true;

    return true;
}

bool GpioTest::startInputInterrupt() {
    const esp_err_t installErr = gpio_install_isr_service(0);
    if (installErr != ESP_OK && installErr != ESP_ERR_INVALID_STATE) {
        m_result.setFailure(TestError::HARDWARE_ERROR, "Failed to install GPIO ISR service");

        return false;
    }

    const esp_err_t addErr = gpio_isr_handler_add(m_config.pin, interruptHandler, this);
    if (addErr != ESP_OK) {
        m_result.setFailure(TestError::HARDWARE_ERROR, "Failed to add GPIO ISR handler");

        return false;
    }

    m_isrAttached = true;

    const esp_err_t enableErr = gpio_intr_enable(m_config.pin);
    if (enableErr != ESP_OK) {
        m_result.setFailure(TestError::HARDWARE_ERROR, "Failed to enable GPIO interrupt");

        return false;
    }

    return true;
}

void GpioTest::processInput() {
    uint32_t interruptCount = 0;

    portENTER_CRITICAL(&m_isrMux);

    interruptCount = m_interruptCount;

    portEXIT_CRITICAL(&m_isrMux);

    if (interruptCount >= m_config.expected_interrupts) {
        m_result.setSuccess("GPIO input interrupt count matched expected value");

        finish(TestState::PASSED);

        return;
    }

    const TickType_t now = xTaskGetTickCount();

    if ((now - m_startTick) >= m_timeoutTicks) {
        m_result.setFailure(TestError::TIMEOUT, "GPIO input interrupt count did not reach expected value within timeout");

        finish(TestState::FAILED);
    }
}

void GpioTest::processOutput() {
    const TickType_t now = xTaskGetTickCount();

    if ((now - m_lastProcessTick) < m_intervalTicks) {
        return;
    }

    m_lastProcessTick = now;

    m_expectedLevel = !m_expectedLevel;

    const int expectedLevel = m_expectedLevel ? 1 : 0;

    const esp_err_t err = gpio_set_level(m_config.pin, expectedLevel);

    if (err != ESP_OK) {
        m_result.setFailure(TestError::HARDWARE_ERROR, "GPIO write failed");

        finish(TestState::FAILED);

        return;
    }

    const int actualLevel = gpio_get_level(m_config.pin);
    if (actualLevel < 0 || actualLevel != expectedLevel) {
        m_result.setFailure(TestError::VALIDATION_FAILED, "GPIO output readback mismatch");

        finish(TestState::FAILED);

        return;
    }

    if (!m_expectedLevel) {
        ++m_completedCycles;
    }

    if (m_completedCycles >= m_config.cycles) {
        m_result.setSuccess("GPIO output readback passed");

        finish(TestState::PASSED);
    }
}

bool GpioTest::configurePin() {
    switch (m_config.mode) {
    case Mode::INPUT:

        return configureInput();
    case Mode::OUTPUT:
        return configureOutput();
    default:
        m_result.setFailure(TestError::INVALID_ARGUMENT, "Invalid GPIO test mode");
        return false;
    }
}

bool GpioTest::start() {
    if (m_state == TestState::RUNNING) {
        return false;
    }

    m_result.reset();

    m_completedCycles = 0;
    m_expectedLevel = false;
    m_interruptCount = 0;
    m_initialized = false;
    m_isrAttached = false;

    if (m_config.mode == Mode::INPUT) {
        if (m_config.expected_interrupts == 0 || m_config.timeout_ms == 0) {
            m_result.setFailure(TestError::INVALID_ARGUMENT, "Invalid GPIO test configuration");

            m_state = TestState::FAILED;

            return false;
        }
    } else if (m_config.mode == Mode::OUTPUT) {
        if (m_config.cycles == 0 || m_config.interval_ms == 0) {
            m_result.setFailure(TestError::INVALID_ARGUMENT, "Invalid GPIO test configuration");

            m_state = TestState::FAILED;

            return false;
        }
    } else {
        m_result.setFailure(TestError::INVALID_ARGUMENT, "Invalid GPIO test mode");

        m_state = TestState::FAILED;

        return false;
    }

    if (!configurePin()) {
        cleanUp();

        m_state = TestState::FAILED;

        return false;
    }

    if (m_config.mode == Mode::INPUT) {
        if (!startInputInterrupt()) {
            cleanUp();

            m_state = TestState::FAILED;

            return false;
        }

        m_timeoutTicks = pdMS_TO_TICKS(m_config.timeout_ms);
        if (m_timeoutTicks == 0) {
            m_timeoutTicks = 1;
        }
    } else if (m_config.mode == Mode::OUTPUT) {
        const esp_err_t err = gpio_set_level(m_config.pin, 0);
        if (err != ESP_OK) {
            m_result.setFailure(TestError::HARDWARE_ERROR, "Failed to initialize output level to LOW");

            cleanUp();

            m_state = TestState::FAILED;

            return false;
        }

        m_intervalTicks = pdMS_TO_TICKS(m_config.interval_ms);
        if (m_intervalTicks == 0) {
            m_intervalTicks = 1;
        }
        m_lastProcessTick = xTaskGetTickCount();
    }

    m_state = TestState::RUNNING;

    return true;
}

void GpioTest::stop() {
    if (m_state != TestState::RUNNING) {
        return;
    }
    m_result.setFailure(TestError::INTERNAL_ERROR, "GPIO test stopped by host");

    finish(TestState::STOPPED);
}

void GpioTest::process() {
    if (m_state != TestState::RUNNING) {
        return;
    }

    switch (m_config.mode) {
    case Mode::INPUT:
        processInput();
        break;
    case Mode::OUTPUT:
        processOutput();
        break;
    default:
        m_result.setFailure(TestError::INTERNAL_ERROR, "Invalid GPIO test mode");
        finish(TestState::FAILED);
        break;
    }
}

void GpioTest::finish(const TestState state) {
    m_state = state;

    if (m_initialized) {
        gpio_set_level(m_config.pin, 0);
    }
}

void GpioTest::cleanUp() {
    if (m_isrAttached) {
        gpio_intr_disable(m_config.pin);
        gpio_isr_handler_remove(m_config.pin);
        m_isrAttached = false;
    }

    if (m_initialized && m_config.mode == Mode::OUTPUT) {
        gpio_set_level(m_config.pin, 0);
    }

    m_initialized = false;
}

void IRAM_ATTR GpioTest::interruptHandler(void *arg) {
    auto *self = static_cast<GpioTest *>(arg);

    portENTER_CRITICAL_ISR(&self->m_isrMux);

    ++self->m_interruptCount;

    portEXIT_CRITICAL_ISR(&self->m_isrMux);
}

TestState GpioTest::state() const { return m_state; }

const TestResult &GpioTest::result() const { return m_result; }