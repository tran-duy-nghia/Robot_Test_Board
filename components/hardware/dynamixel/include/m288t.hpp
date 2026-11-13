#pragma once

#include <cstddef>
#include <cstdint>

#include "dynamixel_transport.hpp"

class M288T {
public:
    static constexpr uint16_t MODEL_NUMBER_XL330_M288_T = 1200;

    enum class Register : uint16_t {
        MODEL_NUMBER = 0,
        FIRMWARE_VERSION = 6,
        ID = 7,
        BAUD_RATE = 8,
        HARDWARE_ERROR_STATUS = 70
    };

    struct Config {
        SingleWireTransport::Config transport{};
        uint8_t id = 1;
        uint16_t expected_model_number = MODEL_NUMBER_XL330_M288_T;
    };

    struct DeviceInfo {
        uint16_t model_number = 0;
        uint8_t firmware_version = 0;
        uint8_t id = 0;
        uint8_t baud_rate = 0;
        uint8_t hardware_error_status = 0;
        const char *failed_step = "";
    };

    explicit M288T(const Config &config);
    ~M288T() = default;

    M288T(const M288T &) = delete;
    M288T &operator=(const M288T &) = delete;

    void configure(const Config &config);
    esp_err_t init();
    void deinit();

    esp_err_t ping(uint16_t &modelNumber, uint8_t &firmwareVersion);
    esp_err_t readRegister(uint16_t address, uint16_t length, uint8_t *data, std::size_t capacity);
    esp_err_t identify(DeviceInfo &info);

private:
    Config m_config{};
    SingleWireTransport m_transport{};
};
