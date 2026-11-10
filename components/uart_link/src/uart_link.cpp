#include "uart_link.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"

esp_err_t UartLink::init(const Config &config) {
    if (m_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    m_config = config;

    uart_config_t uart_config = {};

    uart_config.baud_rate = config.baudrate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    ESP_RETURN_ON_ERROR(
        uart_param_config(
            config.port,
            &uart_config),
        "UART",
        "UART configuration failed");

    ESP_RETURN_ON_ERROR(
        uart_set_pin(
            config.port,
            config.tx_pin,
            config.rx_pin,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE),
        "UART",
        "UART pin configuration failed");

    ESP_RETURN_ON_ERROR(
        uart_driver_install(
            config.port,
            config.rx_buffer_size,
            config.tx_buffer_size,
            0,
            nullptr,
            0),
        "UART",
        "UART driver installation failed");

    m_rx_buffer.clear();

    m_initialized = true;

    return ESP_OK;
}

esp_err_t UartLink::readLine(
    std::string &line,
    uint32_t timeout_ms) {
    if (!m_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const TickType_t start = xTaskGetTickCount();

    const TickType_t timeout =
        pdMS_TO_TICKS(timeout_ms);

    while (true) {

        const TickType_t elapsed =
            xTaskGetTickCount() - start;

        if (elapsed >= timeout) {
            return ESP_ERR_TIMEOUT;
        }

        const TickType_t remaining =
            timeout - elapsed;

        uint8_t ch = 0;

        const int len = uart_read_bytes(
            m_config.port,
            &ch,
            1,
            remaining);

        if (len <= 0) {
            return ESP_ERR_TIMEOUT;
        }

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {

            line = m_rx_buffer;

            m_rx_buffer.clear();

            return ESP_OK;
        }

        if (m_rx_buffer.size() >=
            m_config.max_line_length) {

            m_rx_buffer.clear();

            return ESP_ERR_INVALID_SIZE;
        }

        m_rx_buffer.push_back(
            static_cast<char>(ch));
    }
}

esp_err_t UartLink::writeLine(
    const std::string &line) {
    if (!m_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const int written = uart_write_bytes(
        m_config.port,
        line.c_str(),
        line.size());

    if (written < 0 ||
        static_cast<size_t>(written) != line.size()) {

        return ESP_FAIL;
    }

    const char newline = '\n';

    const int newline_written = uart_write_bytes(
        m_config.port,
        &newline,
        1);

    if (newline_written != 1) {
        return ESP_FAIL;
    }

    return uart_wait_tx_done(
        m_config.port,
        pdMS_TO_TICKS(1000));
}

void UartLink::deinit() {
    if (!m_initialized) {
        return;
    }

    uart_driver_delete(m_config.port);

    m_rx_buffer.clear();

    m_initialized = false;
}