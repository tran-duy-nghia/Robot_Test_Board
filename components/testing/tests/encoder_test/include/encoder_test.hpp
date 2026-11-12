#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "driver/mcpwm_cap.h"
#include "freertos/FreeRTOS.h"
#include "test_interface.hpp"

class EncoderTest final : public ITest {
public:
    struct Config {
        gpio_num_t ch1;
        gpio_num_t ch2;
        gpio_num_t ch3;

        uint32_t min_pulses_per_channel = 10;
        uint32_t timeout_ms = 5000;
        uint32_t capture_prescale = 1;
    };

    EncoderTest() = default;

    explicit EncoderTest(const Config &config);

    ~EncoderTest() override;

    const char *id() const override;

    const char *name() const override;

    bool start() override;

    void stop() override;

    void process() override;

    TestState state() const override;

    const TestResult &result() const override;

private:
    static constexpr uint8_t CHANNEL_COUNT = 3;

    struct ChannelStats {
        uint32_t pulse_count = 0;
        uint32_t last_capture = 0;
        uint32_t period_ticks = 0;
        bool has_previous_capture = false;
    };

    struct CaptureContext {
        EncoderTest *owner = nullptr;
        uint8_t channel_index = 0;
    };

    static bool onCapture(mcpwm_cap_channel_handle_t channel, const mcpwm_capture_event_data_t *event_data, void *user_data);

    bool validateConfig() const;

    bool initializeCapture();

    bool initializeChannel(uint8_t channelIndex);

    void cleanUp();

    void finish(TestState final_state);

    void failStart(const char *message);

    uint32_t frequencyHz(const ChannelStats &stat) const;

private:
    Config m_config{};

    mcpwm_cap_timer_handle_t m_captureTimer = nullptr;
    mcpwm_cap_channel_handle_t m_captureChannels[CHANNEL_COUNT]{};

    CaptureContext m_captureContexts[CHANNEL_COUNT]{};
    ChannelStats m_channelStats[CHANNEL_COUNT]{};

    bool m_channelEnabled[CHANNEL_COUNT]{};
    bool m_timerEnabled = false;
    bool m_timerStarted = false;

    uint32_t m_captureResolutionHz = 0;

    TickType_t m_startTick = 0;
    TickType_t m_timeoutTicks = 0;

    portMUX_TYPE m_captureMux = portMUX_INITIALIZER_UNLOCKED;
};
