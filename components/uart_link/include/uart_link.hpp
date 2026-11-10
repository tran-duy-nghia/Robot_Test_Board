#pragma once

#include <string>

#include "driver/gpio.h"
#include "driver/uart.h"

#include "esp_err.h"

class UartLink {
public:
    struct Config {
        uart_port_t port = UART_NUM_0;

        gpio_num_t tx_pin = GPIO_NUM_43;
        gpio_num_t rx_pin = GPIO_NUM_44;

        uint32_t baudrate = 115200;

        uint32_t rx_buffer_size = 1024;
        uint32_t tx_buffer_size = 1024;

        size_t max_line_length = 256;
    };

    esp_err_t init(const Config &config);

    esp_err_t readLine(std::string &line, uint32_t timeout_ms = 1000);

    esp_err_t writeLine(const std::string &line);

    void deinit();

private:
    bool m_initialized = false;

    Config m_config;

    std::string m_rx_buffer;
};
