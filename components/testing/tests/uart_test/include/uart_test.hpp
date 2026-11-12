#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

#include "test_interface.hpp"

class UartTest final : public ITest {
public:
    struct Config {
        uart_port_t port;

        gpio_num_t tx_pin;
        gpio_num_t rx_pin;

        uint32_t baudrate = 115200;
        uint32_t timeout_ms = 10000;

        uint16_t expected_bytes = 10;
    };

    UartTest() = default;

    explicit UartTest(const Config &config);

    ~UartTest() override;

    const char *id() const override;

    const char *name() const override;

    bool start() override;

    void stop() override;

    void process() override;

    TestState state() const override;

    const TestResult &result() const override;

private:
    static constexpr uint8_t PATTERN_SIZE = 2;
    static constexpr uint8_t IO_BUFFER_SIZE = 32;

    static constexpr uint8_t TEST_PATTERN[PATTERN_SIZE] = {
        0xA5,
        0x5A};

    bool validateConfig() const;

    bool initializeUART();

    bool transmitData();

    void cleanUp();

    void finish(TestState finalState);

    void failStart(const char *message);

    void failTest(TestError error, const char *message);

private:
    Config m_config{};

    uint8_t *m_txBuffer = nullptr;

    uint16_t m_receivedCount = 0;

    TickType_t m_startTick = 0;
    TickType_t m_timeoutTicks = 0;

    bool m_uartInstalled = false;
};