#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
// #include "driver/uart.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "soft_uart.h"
#include "dynamixel_protocol2.hpp"

class SingleWireTransport {
public:
    struct Config {
        gpio_num_t tx_pin = GPIO_NUM_5;
        gpio_num_t rx_pin = GPIO_NUM_4;

        soft_uart_baudrate_t baudrate = SOFT_UART_57600;
        uint32_t response_timeout_ms = 150;
        uint32_t turnaround_delay_us = 2;

        // 74LVC2G125 OE is active-low. These defaults assume GPIO46
        // selects TX when LOW and RX when HIGH through the board buffer logic.
        uint8_t tx_direction_level = 0;
        uint8_t rx_direction_level = 1;
    };

    SingleWireTransport() = default;
    ~SingleWireTransport();

    SingleWireTransport(const SingleWireTransport &) = delete;
    SingleWireTransport &operator=(const SingleWireTransport &) = delete;

    esp_err_t init(const Config &config);
    void deinit();

    esp_err_t transact(
        uint8_t id,
        uint8_t instruction,
        const uint8_t *parameters,
        std::size_t parameterCount,
        StatusPacket &response,
        uint32_t timeoutMs = 0);

    bool isInitialized() const { return m_initialized; }

private:
    esp_err_t receiveStatus(StatusPacket &response, uint32_t timeoutMs);
    esp_err_t readExact(uint8_t *buffer, std::size_t length, TickType_t deadline);
    esp_err_t readByte(uint8_t &value, TickType_t deadline);

private:
    Config m_config{};
    soft_uart_port_t m_port = nullptr;
    bool m_initialized = false;
    bool m_uartInstalled = false;
};

