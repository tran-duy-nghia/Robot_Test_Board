#pragma once

#include "test_interface.hpp"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include <array>
#include <cstdint>

class PwmTest final : public ITest {
public:
    static constexpr std::size_t CHANNEL_COUNT = 4;

    struct Config {
        std::array<gpio_num_t, CHANNEL_COUNT> pins{
            GPIO_NUM_19,
            GPIO_NUM_20,
            GPIO_NUM_21,
            GPIO_NUM_35,
        };

        uint32_t frequency_hz = 1000;
        uint8_t resolution_bits = 10;
        uint8_t duty_percent = 50;
        uint32_t channel_duration_ms = 1000;
    };

    PwmTest() = default;
    explicit PwmTest(const Config &config);
    ~PwmTest() override;

    const char *id() const override;
    const char *name() const override;

    bool start() override;
    void stop() override;
    void process() override;

    TestState state() const override;
    const TestResult &result() const override;

private:
    bool validateConfig();
    bool configureTimer();
    bool configureChannels();
    bool activateChannel(std::size_t index);

    void finish(TestState finalState);
    void cleanUp();
    void failStart(const char *message);

private:
    static constexpr ledc_mode_t LEDC_MODE = LEDC_LOW_SPEED_MODE;
    static constexpr ledc_timer_t LEDC_TIMER = LEDC_TIMER_0;

    Config m_config{};
    std::array<bool, CHANNEL_COUNT> m_channelConfigured{};

    std::size_t m_activeChannel = 0;
    TickType_t m_channelStartTick = 0;
    TickType_t m_channelDurationTicks = 0;

    bool m_timerConfigured = false;
};
