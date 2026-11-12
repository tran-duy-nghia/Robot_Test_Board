#pragma once

#include "test_interface.hpp"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"

#include <cstdint>

class I2CTest final : public ITest {
public:
    struct Config {
        i2c_port_t port = I2C_NUM_0;

        gpio_num_t sda_pin = GPIO_NUM_8;
        gpio_num_t scl_pin = GPIO_NUM_9;

        uint32_t frequency_hz = 100000;

        uint32_t probe_timeout_ms = 20;

        uint32_t interval_ms = 10;

        bool enable_internal_pullup = true;
    };

    I2CTest() = default;

    explicit I2CTest(const Config &config);

    ~I2CTest() override;

    const char *id() const override;

    const char *name() const override;

    bool start() override;

    void stop() override;

    void process() override;

    TestState state() const override;

    const TestResult &result() const override;

private:
    bool configureI2C();

    void finish(TestState state);

private:
    Config m_config{};

    i2c_master_bus_handle_t m_busHandle = nullptr;

    uint8_t m_nextAddress = 0x08;

    uint8_t m_foundDevices = 0;

    TickType_t m_lastProcessTick = 0;
    TickType_t m_intervalTicks = 0;

    uint32_t m_completedCycles = 0;
};