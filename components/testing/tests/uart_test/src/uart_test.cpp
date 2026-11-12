#include "uart_test.hpp"

#include <cstdio>
#include <cstring>
#include <new>

#include "esp_err.h"

UartTest::UartTest(const Config &config)
    : m_config(config) {
}

UartTest::~UartTest() {
    cleanUp();

    delete[] m_txBuffer;
    m_txBuffer = nullptr;
}

const char *UartTest::id() const {
    return "UART";
}

const char *UartTest::name() const {
    return "UART Loopback Test";
}

TestState UartTest::state() const {
    return m_state;
}

const TestResult &UartTest::result() const {
    return m_result;
}

bool UartTest::validateConfig() const {
    if (m_config.port == UART_NUM_0 ||
        m_config.port >= UART_NUM_MAX) {
        return false;
    }

    if (!GPIO_IS_VALID_GPIO(m_config.tx_pin) ||
        !GPIO_IS_VALID_GPIO(m_config.rx_pin)) {
        return false;
    }

    if (m_config.tx_pin == m_config.rx_pin) {
        return false;
    }

    if (m_config.baudrate == 0 ||
        m_config.timeout_ms == 0 ||
        m_config.expected_bytes == 0) {
        return false;
    }

    if (m_config.expected_bytes % PATTERN_SIZE != 0) {
        return false;
    }

    return true;
}

bool UartTest::start() {
    if (m_state == TestState::RUNNING) {
        return false;
    }

    cleanUp();

    m_result.reset();
    m_receivedCount = 0;

    if (!validateConfig()) {
        failStart("Invalid UART configuration");
        return false;
    }

    delete[] m_txBuffer;

    m_txBuffer = new (std::nothrow) uint8_t[m_config.expected_bytes];

    if (m_txBuffer == nullptr) {
        failStart("TX buffer allocation failed");
        return false;
    }

    // Generate A5 5A A5 5A ...
    for (uint16_t i = 0; i < m_config.expected_bytes; i += 2) {
        m_txBuffer[i] = TEST_PATTERN[0];
        m_txBuffer[i + 1] = TEST_PATTERN[1];
    }

    m_timeoutTicks = pdMS_TO_TICKS(m_config.timeout_ms);

    if (m_timeoutTicks == 0) {
        m_timeoutTicks = 1;
    }

    if (!initializeUART()) {
        failStart("UART initialization failed");
        return false;
    }

    // Remove any stale RX data.
    uart_flush_input(m_config.port);

    // Start timeout measurement before transmission.
    m_startTick = xTaskGetTickCount();

    m_state = TestState::RUNNING;
    m_initialized = true;

    if (!transmitData()) {
        failTest(
            TestError::COMMUNICATION_ERROR,
            "UART TX failed");

        return false;
    }

    return true;
}

bool UartTest::initializeUART() {
    uart_config_t uartConfig{};

    uartConfig.baud_rate = static_cast<int>(m_config.baudrate);
    uartConfig.data_bits = UART_DATA_8_BITS;
    uartConfig.parity = UART_PARITY_DISABLE;
    uartConfig.stop_bits = UART_STOP_BITS_1;
    uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uartConfig.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_driver_install(m_config.port, 256, 0, 0, nullptr, 0);

    if (err != ESP_OK) {
        return false;
    }

    m_uartInstalled = true;

    err = uart_param_config(m_config.port, &uartConfig);

    if (err != ESP_OK) {
        cleanUp();
        return false;
    }

    err = uart_set_pin(
        m_config.port,
        m_config.tx_pin,
        m_config.rx_pin,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE);

    if (err != ESP_OK) {
        cleanUp();
        return false;
    }

    return true;
}

bool UartTest::transmitData() {
    if (!m_uartInstalled || m_txBuffer == nullptr) {
        return false;
    }

    const int written = uart_write_bytes(
        m_config.port,
        reinterpret_cast<const char *>(m_txBuffer),
        m_config.expected_bytes);

    return written == static_cast<int>(m_config.expected_bytes);
}

void UartTest::process() {
    if (m_state != TestState::RUNNING) {
        return;
    }

    size_t availableBytes = 0;

    esp_err_t err = uart_get_buffered_data_len(
        m_config.port,
        &availableBytes);

    if (err != ESP_OK) {
        failTest(
            TestError::HARDWARE_ERROR,
            "UART RX buffer query failed");
        return;
    }

    if (availableBytes > 0) {
        uint8_t rxBuffer[IO_BUFFER_SIZE]{};

        const size_t bytesToRead =
            (availableBytes > sizeof(rxBuffer))
                ? sizeof(rxBuffer)
                : availableBytes;

        const int received = uart_read_bytes(
            m_config.port,
            rxBuffer,
            bytesToRead,
            0);

        if (received < 0) {
            failTest(
                TestError::COMMUNICATION_ERROR,
                "UART RX failed");
            return;
        }

        for (int i = 0; i < received; ++i) {
            if (m_receivedCount >= m_config.expected_bytes) {
                failTest(
                    TestError::VALIDATION_FAILED,
                    "Received more bytes than expected");
                return;
            }

            const uint8_t expected = m_txBuffer[m_receivedCount];
            const uint8_t actual = rxBuffer[i];

            if (actual != expected) {
                char message[128];

                std::snprintf(
                    message,
                    sizeof(message),
                    "Data mismatch at byte %u: expected 0x%02X, received 0x%02X",
                    static_cast<unsigned>(m_receivedCount),
                    expected,
                    actual);

                failTest(
                    TestError::VALIDATION_FAILED,
                    message);

                return;
            }

            ++m_receivedCount;
        }
    }

    // Check whether all expected bytes have been received.
    if (m_receivedCount == m_config.expected_bytes) {
        char message[128];

        std::snprintf(
            message,
            sizeof(message),
            "UART loopback passed: TX=%u RX=%u",
            static_cast<unsigned>(m_config.expected_bytes),
            static_cast<unsigned>(m_receivedCount));

        m_result.setSuccess(message);

        finish(TestState::PASSED);
        return;
    }

    // Check timeout.
    const TickType_t elapsed =
        xTaskGetTickCount() - m_startTick;

    if (elapsed >= m_timeoutTicks) {
        char message[128];

        std::snprintf(
            message,
            sizeof(message),
            "UART timeout: expected=%u received=%u",
            static_cast<unsigned>(m_config.expected_bytes),
            static_cast<unsigned>(m_receivedCount));

        failTest(
            TestError::TIMEOUT,
            message);
    }
}

void UartTest::stop() {
    if (m_state != TestState::RUNNING) {
        return;
    }

    m_result.setFailure(
        TestError::INTERNAL_ERROR,
        "UART test stopped by host");

    finish(TestState::STOPPED);
}

void UartTest::finish(TestState finalState) {
    cleanUp();

    m_state = finalState;
    m_initialized = false;
}

void UartTest::failStart(const char *message) {
    cleanUp();

    m_result.setFailure(
        TestError::HARDWARE_ERROR,
        message);

    m_state = TestState::FAILED;
    m_initialized = false;
}

void UartTest::failTest(
    TestError error,
    const char *message) {
    m_result.setFailure(error, message);

    finish(TestState::FAILED);
}

void UartTest::cleanUp() {
    if (m_uartInstalled) {
        uart_driver_delete(m_config.port);
        m_uartInstalled = false;
    }
}