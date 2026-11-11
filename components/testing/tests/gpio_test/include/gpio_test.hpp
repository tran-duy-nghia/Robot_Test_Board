#pragma once

#include "test_interface.hpp"

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"

#include <cstdint>

class GpioTest final : public ITest {
public:
    struct Config {
        gpio_num_t pin = GPIO_NUM_4;

        uint32_t interval_ms = 100;

        uint32_t cycles = 5;
    };

    GpioTest() = default;

    explicit GpioTest(const Config& config);

    ~GpioTest() override = default;

    const char* id() const override;

    const char* name() const override;

    bool start() override;

    void stop() override;

    void process() override;

    TestState state() const override;

    const TestResult& result() const override;

private:
    bool configurePin();

    bool verifyLevel(int expectedLevel);

    void finish(TestState state);

private:
    Config m_config{};

    bool m_expectedLevel = false;
};