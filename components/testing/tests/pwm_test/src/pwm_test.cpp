#include "pwm_test.hpp"

#include "esp_err.h"

PwmTest::PwmTest(const Config &config) : m_config(config) {}

PwmTest::~PwmTest() {
    if (m_state == TestState::RUNNING) {
        stop();
    }
    cleanUp();
}

const char *PwmTest::id() const { return "PWM"; }

const char *PwmTest::name() const { return "PWM Output Test"; }

TestState PwmTest::state() const { return m_state; }

const TestResult &PwmTest::result() const { return m_result; }

bool PwmTest::validateConfig() {
    if (m_config.frequency_hz == 0 || m_config.channel_duration_ms == 0 ||
        m_config.duty_percent == 0 || m_config.duty_percent >= 100 ||
        m_config.resolution_bits == 0 || m_config.resolution_bits > 14) {
        m_result.setFailure(TestError::INVALID_ARGUMENT, "Invalid PWM frequency, duty, resolution or duration");
        return false;
    }

    for (std::size_t i = 0; i < CHANNEL_COUNT; ++i) {
        if (!GPIO_IS_VALID_OUTPUT_GPIO(m_config.pins[i])) {
            m_result.setFailure(TestError::INVALID_ARGUMENT, "PWM channel has an invalid output GPIO");
            return false;
        }

        for (std::size_t j = i + 1; j < CHANNEL_COUNT; ++j) {
            if (m_config.pins[i] == m_config.pins[j]) {
                m_result.setFailure(TestError::INVALID_ARGUMENT, "PWM GPIO pins must be unique");
                return false;
            }
        }
    }

    return true;
}

bool PwmTest::configureTimer() {
    ledc_timer_config_t timerConfig{};
    timerConfig.speed_mode = LEDC_MODE;
    timerConfig.duty_resolution = static_cast<ledc_timer_bit_t>(m_config.resolution_bits);
    timerConfig.timer_num = LEDC_TIMER;
    timerConfig.freq_hz = m_config.frequency_hz;
    timerConfig.clk_cfg = LEDC_AUTO_CLK;

    if (ledc_timer_config(&timerConfig) != ESP_OK) {
        m_result.setFailure(TestError::HARDWARE_ERROR, "LEDC timer configuration failed");
        return false;
    }

    m_timerConfigured = true;
    return true;
}

bool PwmTest::configureChannels() {
    for (std::size_t i = 0; i < CHANNEL_COUNT; ++i) {
        ledc_channel_config_t channelConfig{};
        channelConfig.gpio_num = m_config.pins[i];
        channelConfig.speed_mode = LEDC_MODE;
        channelConfig.channel = static_cast<ledc_channel_t>(i);
        // channelConfig.intr_type = LEDC_INTR_DISABLE;
        channelConfig.timer_sel = LEDC_TIMER;
        channelConfig.duty = 0;
        channelConfig.hpoint = 0;

        if (ledc_channel_config(&channelConfig) != ESP_OK) {
            m_result.setFailure(TestError::HARDWARE_ERROR, "LEDC channel configuration failed");
            return false;
        }

        m_channelConfigured[i] = true;
    }

    return true;
}

bool PwmTest::activateChannel(std::size_t index) {
    const uint32_t maxDuty = (1UL << m_config.resolution_bits) - 1UL;
    const uint32_t duty = (maxDuty * m_config.duty_percent) / 100UL;
    const auto channel = static_cast<ledc_channel_t>(index);

    if (ledc_set_duty(LEDC_MODE, channel, duty) != ESP_OK ||
        ledc_update_duty(LEDC_MODE, channel) != ESP_OK) {
        m_result.setFailure(TestError::HARDWARE_ERROR, "Failed to start PWM output channel");
        return false;
    }

    m_channelStartTick = xTaskGetTickCount();
    return true;
}

bool PwmTest::start() {
    if (m_state == TestState::RUNNING) {
        return false;
    }

    m_result.reset();
    cleanUp();
    m_channelConfigured.fill(false);
    m_activeChannel = 0;
    m_timerConfigured = false;

    if (!validateConfig() || !configureTimer() || !configureChannels()) {
        cleanUp();
        m_state = TestState::FAILED;
        return false;
    }

    m_channelDurationTicks = pdMS_TO_TICKS(m_config.channel_duration_ms);
    if (m_channelDurationTicks == 0) {
        m_channelDurationTicks = 1;
    }

    if (!activateChannel(m_activeChannel)) {
        cleanUp();
        m_state = TestState::FAILED;
        return false;
    }

    m_state = TestState::RUNNING;
    return true;
}

void PwmTest::process() {
    if (m_state != TestState::RUNNING) {
        return;
    }

    const TickType_t now = xTaskGetTickCount();
    if ((now - m_channelStartTick) < m_channelDurationTicks) {
        return;
    }

    // End the current output before moving to the next connector pin.
    const auto currentChannel = static_cast<ledc_channel_t>(m_activeChannel);
    if (ledc_set_duty(LEDC_MODE, currentChannel, 0) != ESP_OK ||
        ledc_update_duty(LEDC_MODE, currentChannel) != ESP_OK) {
        m_result.setFailure(TestError::HARDWARE_ERROR, "Failed to stop current PWM output channel");
        finish(TestState::FAILED);
        return;
    }

    ++m_activeChannel;
    if (m_activeChannel >= CHANNEL_COUNT) {
        m_result.setSuccess("LEDC configured and all four PWM outputs were exercised");
        finish(TestState::PASSED);
        return;
    }

    if (!activateChannel(m_activeChannel)) {
        finish(TestState::FAILED);
    }
}

void PwmTest::stop() {
    if (m_state != TestState::RUNNING) {
        return;
    }

    m_result.setFailure(TestError::INTERNAL_ERROR, "PWM test stopped by host");
    finish(TestState::STOPPED);
}

void PwmTest::finish(TestState finalState) {
    cleanUp();
    m_state = finalState;
}

void PwmTest::cleanUp()
{
    for (std::size_t i = 0; i < CHANNEL_COUNT; ++i) {
        if (!m_channelConfigured[i]) {
            continue;
        }

        const auto channel = static_cast<ledc_channel_t>(i);
        const auto pin = m_config.pins[i];

        // Stop PWM and set idle level LOW.
        (void)ledc_stop(LEDC_MODE, channel, 0);

        // Deconfigure LEDC channel to release GPIO reservation.
        ledc_channel_config_t channelConfig{};
        channelConfig.speed_mode = LEDC_MODE;
        channelConfig.channel = channel;
        channelConfig.deconfigure = true;

        (void)ledc_channel_config(&channelConfig);

        // Keep GPIO in output LOW state after releasing LEDC.
        (void)gpio_set_level(pin, 0);
        (void)gpio_set_direction(pin, GPIO_MODE_OUTPUT);

        m_channelConfigured[i] = false;
    }

    m_timerConfigured = false;
}

void PwmTest::failStart(const char *message) {
    m_result.setFailure(TestError::HARDWARE_ERROR, message);
    cleanUp();
    m_state = TestState::FAILED;
}
