#include "m288t.hpp"

#include <cstring>

namespace {
constexpr uint8_t DXL_BAUD_57600 = 1;
}

M288T::M288T(const Config &config)
    : m_config(config) {
}

void M288T::configure(const Config &config) {
    if (!m_transport.isInitialized()) {
        m_config = config;
    }
}

esp_err_t M288T::init() {
    return m_transport.init(m_config.transport);
}

void M288T::deinit() {
    m_transport.deinit();
}

esp_err_t M288T::ping(uint16_t &modelNumber, uint8_t &firmwareVersion) {
    StatusPacket status{};

    const esp_err_t err = m_transport.transact(
        m_config.id,
        Protocol2::INST_PING,
        nullptr,
        0,
        status);

    if (err != ESP_OK) {
        return err;
    }

    // Protocol 2.0 Ping Status Packet parameters:
    // Model Number (2 bytes, little-endian) + Firmware Version (1 byte).
    if (status.parameter_count != 3) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    modelNumber = static_cast<uint16_t>(status.parameters[0]) |
                  (static_cast<uint16_t>(status.parameters[1]) << 8);
    firmwareVersion = status.parameters[2];

    return ESP_OK;
}

esp_err_t M288T::readRegister(
    uint16_t address,
    uint16_t length,
    uint8_t *data,
    std::size_t capacity) {

    if (data == nullptr || length == 0 || length > capacity) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t parameters[4] = {
        static_cast<uint8_t>(address & 0xFF),
        static_cast<uint8_t>((address >> 8) & 0xFF),
        static_cast<uint8_t>(length & 0xFF),
        static_cast<uint8_t>((length >> 8) & 0xFF)
    };

    StatusPacket status{};
    const esp_err_t err = m_transport.transact(
        m_config.id,
        Protocol2::INST_READ,
        parameters,
        sizeof(parameters),
        status);

    if (err != ESP_OK) {
        return err;
    }

    if (status.parameter_count != length) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    std::memcpy(data, status.parameters, length);
    return ESP_OK;
}

esp_err_t M288T::identify(DeviceInfo &info) {
    info = {};

    uint16_t pingModel = 0;
    uint8_t pingFirmware = 0;

    info.failed_step = "PING";
    esp_err_t err = ping(pingModel, pingFirmware);
    if (err != ESP_OK) {
        return err;
    }

    info.failed_step = "READ_MODEL_NUMBER";
    uint8_t modelBytes[2]{};
    err = readRegister(
        static_cast<uint16_t>(Register::MODEL_NUMBER),
        sizeof(modelBytes),
        modelBytes,
        sizeof(modelBytes));
    if (err != ESP_OK) {
        return err;
    }

    info.model_number = static_cast<uint16_t>(modelBytes[0]) |
                        (static_cast<uint16_t>(modelBytes[1]) << 8);

    uint8_t value = 0;

    info.failed_step = "READ_FIRMWARE_VERSION";
    err = readRegister(
        static_cast<uint16_t>(Register::FIRMWARE_VERSION),
        1,
        &value,
        sizeof(value));
    if (err != ESP_OK) {
        return err;
    }
    info.firmware_version = value;

    info.failed_step = "READ_ID";
    err = readRegister(
        static_cast<uint16_t>(Register::ID),
        1,
        &info.id,
        sizeof(info.id));
    if (err != ESP_OK) {
        return err;
    }

    info.failed_step = "READ_BAUD_RATE";
    err = readRegister(
        static_cast<uint16_t>(Register::BAUD_RATE),
        1,
        &info.baud_rate,
        sizeof(info.baud_rate));
    if (err != ESP_OK) {
        return err;
    }

    info.failed_step = "READ_HARDWARE_ERROR_STATUS";
    err = readRegister(
        static_cast<uint16_t>(Register::HARDWARE_ERROR_STATUS),
        1,
        &info.hardware_error_status,
        sizeof(info.hardware_error_status));
    if (err != ESP_OK) {
        return err;
    }

    // Ping and the Model Number register must agree.
    if (pingModel != info.model_number) {
        info.failed_step = "VALIDATE_MODEL_NUMBER";
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (info.model_number != m_config.expected_model_number) {
        info.failed_step = "MODEL_NOT_XL330_M288_T";
        return ESP_ERR_NOT_FOUND;
    }

    if (info.id != m_config.id || info.baud_rate != DXL_BAUD_57600) {
        info.failed_step = "VALIDATE_ID_OR_BAUD_RATE";
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (pingFirmware != info.firmware_version) {
        info.failed_step = "VALIDATE_FIRMWARE_VERSION";
        return ESP_ERR_INVALID_RESPONSE;
    }

    info.failed_step = "";
    return ESP_OK;
}
