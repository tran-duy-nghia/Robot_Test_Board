#include "encoder_test.hpp"

#include "esp_err.h"
#include <cstdio>

EncoderTest::EncoderTest(const Config &config) : m_config(config) {}

EncoderTest::~EncoderTest() { cleanUp(); }

const char *EncoderTest::id() const { return "ENCODER"; }

const char *EncoderTest::name() const { return "Encoder Test"; }

TestState EncoderTest::state() const { return m_state; }

const TestResult &EncoderTest::result() const { return m_result; }

bool EncoderTest::validateConfig() const {
    if (!GPIO_IS_VALID_GPIO(m_config.ch1) || !GPIO_IS_VALID_GPIO(m_config.ch2) || !GPIO_IS_VALID_GPIO(m_config.ch3)) {
        return false;
    }

    if (m_config.ch1 == m_config.ch2 || m_config.ch1 == m_config.ch3 || m_config.ch2 == m_config.ch3) {
        return false;
    }

    if (m_config.min_pulses_per_channel == 0 || m_config.timeout_ms == 0 || m_config.capture_prescale == 0) {
        return false;
    }

    return true;
}

bool EncoderTest::start() {
    if (m_state == TestState::RUNNING) {
        return false;
    }

    cleanUp();

    m_result.reset();

    for (auto &stats : m_channelStats) {
        stats = {};
    }

    if (!validateConfig()) {
        m_result.setFailure(TestError::INVALID_ARGUMENT, "Invalid encoder configuration");
        m_state = TestState::FAILED;
        return false;
    }

    m_timeoutTicks = pdMS_TO_TICKS(m_config.timeout_ms);
    if (m_timeoutTicks == 0) {
        m_timeoutTicks = 1;
    }

    if (!initializeCapture()) {
        cleanUp();
        m_result.setFailure(TestError::HARDWARE_ERROR, "MCPWM capture initialization failed");
        m_state = TestState::FAILED;
        return false;
    }

    m_startTick = xTaskGetTickCount();
    m_state = TestState::RUNNING;
    m_initialized = true;

    return true;
}

bool EncoderTest::initializeCapture() {
    mcpwm_capture_timer_config_t timerConfig{};
    timerConfig.group_id = 0;
    timerConfig.clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT;

    esp_err_t err = mcpwm_new_capture_timer(&timerConfig, &m_captureTimer);
    if (err != ESP_OK) {
        return false;
    }

    for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
        if (!initializeChannel(i)) {
            return false;
        }
    }

    err = mcpwm_capture_timer_enable(m_captureTimer);
    if (err != ESP_OK) {
        return false;
    }

    m_timerEnabled = true;

    err = mcpwm_capture_timer_get_resolution(m_captureTimer, &m_captureResolutionHz);
    if (err != ESP_OK) {
        return false;
    }

    err = mcpwm_capture_timer_start(m_captureTimer);
    if (err != ESP_OK) {
        return false;
    }

    m_timerStarted = true;

    return true;
}

bool EncoderTest::initializeChannel(uint8_t channelIndex) {
    const gpio_num_t pins[CHANNEL_COUNT] = {m_config.ch1, m_config.ch2, m_config.ch3};

    mcpwm_capture_channel_config_t channelConfig{};
    channelConfig.gpio_num = pins[channelIndex];
    channelConfig.prescale = m_config.capture_prescale;

    channelConfig.flags.pos_edge = true;
    channelConfig.flags.neg_edge = false;

    esp_err_t err = mcpwm_new_capture_channel(m_captureTimer, &channelConfig, &m_captureChannels[channelIndex]);
    if (err != ESP_OK) {
        return false;
    }

    m_captureContexts[channelIndex].owner = this;
    m_captureContexts[channelIndex].channel_index = channelIndex;

    mcpwm_capture_event_callbacks_t callbacks{};

    callbacks.on_cap = onCapture;

    err = mcpwm_capture_channel_register_event_callbacks(m_captureChannels[channelIndex], &callbacks, &m_captureContexts[channelIndex]);
    if (err != ESP_OK) {
        return false;
    }

    err = mcpwm_capture_channel_enable(m_captureChannels[channelIndex]);
    if (err != ESP_OK) {
        return false;
    }

    m_channelEnabled[channelIndex] = true;

    return true;
}

bool EncoderTest::onCapture(mcpwm_cap_channel_handle_t channel, const mcpwm_capture_event_data_t *event_data, void *user_data) {
    (void)channel;

    if (event_data == nullptr || user_data == nullptr) {
        return false;
    }

    auto *context = static_cast<CaptureContext *>(user_data);
    EncoderTest *self = context->owner;

    if (self == nullptr || context->channel_index >= CHANNEL_COUNT) {
        return false;
    }

    const uint8_t index = context->channel_index;

    portENTER_CRITICAL_ISR(&self->m_captureMux);

    ChannelStats &stats = self->m_channelStats[index];

    const uint32_t currentCapture = event_data->cap_value;

    if (stats.has_previous_capture) {
        stats.period_ticks = currentCapture - stats.last_capture;
    }

    stats.last_capture = currentCapture;
    stats.has_previous_capture = true;
    ++stats.pulse_count;

    portEXIT_CRITICAL_ISR(&self->m_captureMux);

    return false;
}

uint32_t EncoderTest::frequencyHz(const ChannelStats &stats) const {
    if (stats.period_ticks == 0 || m_captureResolutionHz == 0) {
        return 0;
    }

    return m_captureResolutionHz / stats.period_ticks;
}

void EncoderTest::process() {
    if (m_state != TestState::RUNNING) {
        return;
    }

    ChannelStats snapShot[CHANNEL_COUNT]{};

    portENTER_CRITICAL(&m_captureMux);

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        snapShot[i] = m_channelStats[i];
    }

    portEXIT_CRITICAL(&m_captureMux);

    const bool ch1Passed = snapShot[0].pulse_count >= m_config.min_pulses_per_channel;
    const bool ch2Passed = snapShot[1].pulse_count >= m_config.min_pulses_per_channel;
    const bool ch3Passed = snapShot[2].pulse_count >= m_config.min_pulses_per_channel;

    if (ch1Passed && ch2Passed && ch3Passed) {
        char message[128];
        std::snprintf(message, sizeof(message), "CH1=%lu/%lu %luHz CH2=%lu/%lu %luHz CH3=%lu/%lu %luHz",
                      static_cast<unsigned long>(snapShot[0].pulse_count), static_cast<unsigned long>(m_config.min_pulses_per_channel),
                      static_cast<unsigned long>(frequencyHz(snapShot[0])), static_cast<unsigned long>(snapShot[1].pulse_count),
                      static_cast<unsigned long>(m_config.min_pulses_per_channel), static_cast<unsigned long>(frequencyHz(snapShot[1])),
                      static_cast<unsigned long>(snapShot[2].pulse_count), static_cast<unsigned long>(m_config.min_pulses_per_channel),
                      static_cast<unsigned long>(frequencyHz(snapShot[2])));

        m_result.setSuccess(message);
        finish(TestState::PASSED);

        return;
    }

    const TickType_t elapsed = xTaskGetTickCount() - m_startTick;

    if (elapsed >= m_timeoutTicks) {
        char message[128];

        std::snprintf(message, sizeof(message), "Encoder timeout: CH1=%lu CH2=%lu CH3=%lu",
                      static_cast<unsigned long>(snapShot[0].pulse_count), static_cast<unsigned long>(snapShot[1].pulse_count),
                      static_cast<unsigned long>(snapShot[2].pulse_count));

        m_result.setFailure(TestError::VALIDATION_FAILED, message);
        finish(TestState::FAILED);
    }
}

void EncoderTest::stop() {
    if (m_state != TestState::RUNNING) {
        return;
    }

    finish(TestState::STOPPED);
}

void EncoderTest::finish(TestState final_state) {
    cleanUp();

    m_state = final_state;

    m_initialized = false;
}

void EncoderTest::failStart(const char *message) {
    cleanUp();

    m_result.setFailure(TestError::HARDWARE_ERROR, message);

    m_state = TestState::FAILED;
}

void EncoderTest::cleanUp() {
    // 1. Stop capture timer
    if (m_timerStarted && m_captureTimer != nullptr) {
        const esp_err_t err = mcpwm_capture_timer_stop(m_captureTimer);

        if (err == ESP_OK) {
            m_timerStarted = false;
        }
    }

    // 2. Disable all capture channels
    for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
        if (m_channelEnabled[i] && m_captureChannels[i] != nullptr) {
            const esp_err_t err = mcpwm_capture_channel_disable(m_captureChannels[i]);

            if (err == ESP_OK) {
                m_channelEnabled[i] = false;
            }
        }
    }

    // 3. Delete capture channels FIRST
    for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
        if (m_captureChannels[i] != nullptr) {
            const esp_err_t err = mcpwm_del_capture_channel(m_captureChannels[i]);

            if (err == ESP_OK) {
                m_captureChannels[i] = nullptr;
            } else {
                printf("[ENCODER] Delete channel %u failed: %s\n", i, esp_err_to_name(err));
            }
        }
    }

    // 4. Disable capture timer
    if (m_timerEnabled && m_captureTimer != nullptr) {
        const esp_err_t err = mcpwm_capture_timer_disable(m_captureTimer);

        if (err == ESP_OK) {
            m_timerEnabled = false;
        } else {
            printf("[ENCODER] Disable timer failed: %s\n", esp_err_to_name(err));
        }
    }

    // 5. Delete capture timer LAST
    if (m_captureTimer != nullptr) {
        const esp_err_t err = mcpwm_del_capture_timer(m_captureTimer);

        if (err == ESP_OK) {
            m_captureTimer = nullptr;
        } else {
            printf("[ENCODER] Delete timer failed: %s\n", esp_err_to_name(err));
        }
    }

    m_captureResolutionHz = 0;
}