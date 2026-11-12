#pragma once

#include "test_interface.hpp"

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"

#include <cstdint>

class GpioTest final : public ITest {
public:
    enum class Mode : uint8_t {
        INPUT = 0,
        OUTPUT
    };
    struct Config {
        gpio_num_t pin = GPIO_NUM_4;

        Mode mode = Mode::INPUT;

        // Input configuration
        bool pull_up = false;
        bool pull_down = false;

        gpio_int_type_t interrupt_type = GPIO_INTR_POSEDGE;

        uint32_t expected_interrupts = 5;
        uint32_t timeout_ms = 10000;

        // Output configuration
        uint32_t interval_ms = 100;
        uint32_t cycles = 5;
    };

    GpioTest() = default;

    explicit GpioTest(const Config &config);

    ~GpioTest() override;

    const char *id() const override;

    const char *name() const override;

    bool start() override;

    void stop() override;

    void process() override;

    TestState state() const override;

    const TestResult &result() const override;

private:
    bool configureInput();

    bool configureOutput();

    bool startInputInterrupt();

    void processInput();

    void processOutput();

    bool configurePin();

    void finish(TestState state);

    void cleanUp();

    static void IRAM_ATTR interruptHandler(void *arg);

private:
    Config m_config{};

    portMUX_TYPE m_isrMux = portMUX_INITIALIZER_UNLOCKED;
    uint32_t m_interruptCount = 0;

    bool m_isrAttached = false;
    bool m_expectedLevel = false;

    TickType_t m_startTick = 0;
    TickType_t m_timeoutTicks = 0;
    TickType_t m_lastProcessTick = 0;
    TickType_t m_intervalTicks = 0;

    uint32_t m_completedCycles = 0;
};