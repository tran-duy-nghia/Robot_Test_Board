#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "uart_link.hpp"

#include "esp_err.h"

extern "C" void app_main(void) {
    static UartLink uart;

    UartLink::Config config;

    config.port = UART_NUM_0;

    config.tx_pin = GPIO_NUM_43;
    config.rx_pin = GPIO_NUM_44;

    config.baudrate = 115200;

    config.max_line_length = 256;

    ESP_ERROR_CHECK(
        uart.init(config));

    uart.writeLine("HELLO HTKL");

    while (true) {

        std::string command;

        const esp_err_t err = uart.readLine(
            command,
            1000);

        if (err == ESP_ERR_TIMEOUT) {
            continue;
        }

        if (err != ESP_OK) {
            uart.writeLine("ERROR UART_RX");
            continue;
        }

        if (command == "PING") {

            uart.writeLine("PONG");
        } else if (command == "HELP") {

            uart.writeLine("COMMANDS:");
            uart.writeLine("PING");
            uart.writeLine("HELP");
            uart.writeLine("ECHO <TEXT>");
        } else if (command.rfind("ECHO ", 0) == 0) {

            uart.writeLine(command);
        } else {

            uart.writeLine("ERROR UNKNOWN_COMMAND");
        }
    }
}