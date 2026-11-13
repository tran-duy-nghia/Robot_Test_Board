#include "dynamixel_transport.hpp"

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

namespace {
constexpr std::size_t RX_BUFFER_SIZE = 512;
constexpr std::size_t MAX_STATUS_PACKET_SIZE = 7 + MAX_PACKET_SIZE;
} // namespace

SingleWireTransport::~SingleWireTransport() { deinit(); }

esp_err_t SingleWireTransport::init(const Config &config) {
    if (m_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!GPIO_IS_VALID_GPIO(config.tx_pin) || !GPIO_IS_VALID_GPIO(config.rx_pin) || config.tx_pin == config.rx_pin ||
        config.baudrate == SOFT_UART_BAUD_END || config.response_timeout_ms == 0 ||
        config.tx_direction_level > 1 || config.rx_direction_level > 1 || config.tx_direction_level == config.rx_direction_level) {
        return ESP_ERR_INVALID_ARG;
    }

    m_config = config;

    soft_uart_config_t uartConfig = {
        .tx_pin = static_cast<uint32_t>(m_config.tx_pin), 
        .rx_pin = static_cast<uint32_t>(m_config.rx_pin), 
        .baudrate = m_config.baudrate
    };

    esp_err_t err = soft_uart_new(&uartConfig, &m_port);
    if (err != ESP_OK) {
        deinit();
        return err;
    }
    m_uartInstalled = true;
    m_initialized = true;
    
    return ESP_OK;
}

void SingleWireTransport::deinit() {
    if (m_uartInstalled) {
        soft_uart_del(m_port);
        m_uartInstalled = false;
    }

    m_initialized = false;
}

esp_err_t SingleWireTransport::transact(uint8_t id, uint8_t instruction, const uint8_t *parameters, std::size_t parameterCount,
                                        StatusPacket &response, uint32_t timeoutMs) {

    if (!m_initialized || !m_uartInstalled) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t txPacket[MAX_PACKET_SIZE]{};
    std::size_t txLength = 0;

    if (!Protocol2::buildInstruction(id, instruction, parameters, parameterCount, txPacket, sizeof(txPacket), txLength)) {
        return ESP_ERR_INVALID_ARG;
    }

    const int written = soft_uart_send(m_port, txPacket, txLength);

    if (written != static_cast<int>(txLength)) {
        return ESP_FAIL;
    }

    const uint32_t effectiveTimeout = timeoutMs == 0 ? m_config.response_timeout_ms : timeoutMs;

    esp_err_t err = receiveStatus(response, effectiveTimeout);
    if (err != ESP_OK) {
        return err;
    }

    return response.id == id ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

esp_err_t SingleWireTransport::readByte(uint8_t &value, TickType_t deadline) {
    const TickType_t now = xTaskGetTickCount();
    if (now >= deadline) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = soft_uart_receive(m_port, &value, 1);

    return err;
}

esp_err_t SingleWireTransport::readExact(uint8_t *buffer, std::size_t length, TickType_t deadline) {

    for (std::size_t i = 0; i < length; ++i) {
        const esp_err_t err = readByte(buffer[i], deadline);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

esp_err_t SingleWireTransport::receiveStatus(StatusPacket &response, uint32_t timeoutMs) {

    uint8_t packet[MAX_STATUS_PACKET_SIZE]{};
    const TickType_t timeoutTicks = pdMS_TO_TICKS(timeoutMs);
    const TickType_t deadline = xTaskGetTickCount() + (timeoutTicks == 0 ? 1 : timeoutTicks);

    static constexpr uint8_t header[4] = {0xFF, 0xFF, 0xFD, 0x00};

    // The RX path may capture the packet just transmitted (local echo).
    // Ignore complete non-Status packets and continue until a valid 0x55
    // Status Packet is found or the common deadline expires.
    while (xTaskGetTickCount() < deadline) {
        uint8_t matched = 0;
        while (matched < sizeof(header)) {
            uint8_t value = 0;
            const esp_err_t err = readByte(value, deadline);
            if (err != ESP_OK) {
                return err;
            }

            if (value == header[matched]) {
                packet[matched++] = value;
            } else if (matched >= 2 && value == 0xFF) {
                matched = 2;
                packet[0] = 0xFF;
                packet[1] = 0xFF;
            } else {
                matched = (value == 0xFF) ? 1 : 0;
                if (matched == 1) {
                    packet[0] = 0xFF;
                }
            }
        }

        // ID + LENGTH_L + LENGTH_H
        esp_err_t err = readExact(packet + 4, 3, deadline);
        if (err != ESP_OK) {
            return err;
        }

        const std::size_t lengthField = static_cast<std::size_t>(packet[5]) | (static_cast<std::size_t>(packet[6]) << 8);

        if (lengthField < 3 || 7 + lengthField > sizeof(packet)) {
            // Bad candidate. Resume header synchronization.
            continue;
        }

        err = readExact(packet + 7, lengthField, deadline);
        if (err != ESP_OK) {
            return err;
        }

        const std::size_t packetLength = 7 + lengthField;
        StatusPacket candidate{};
        if (!Protocol2::parseStatus(packet, packetLength, candidate)) {
            // Usually the echoed Instruction Packet; skip it.
            continue;
        }

        response = candidate;
        if (response.error != 0) {
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}
