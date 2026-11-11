#include <cstdio>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"

#include "dispatcher.hpp"
#include "parser.hpp"
#include "uart_link.hpp"

#include "test_manager.hpp"
#include "test_registry.hpp"

#include "gpio_test.hpp"

// --------------------------------------------------
// Application context
// --------------------------------------------------

struct TestHandlerContext {
    UartLink *uart = nullptr;

    TestManager *manager = nullptr;

    bool resultPending = false;
};

// --------------------------------------------------
// Utility functions
// --------------------------------------------------

static const char *parseErrorToString(ParseError error) {
    switch (error) {
    case ParseError::EmptyInput:
        return "EMPTY_COMMAND";

    case ParseError::InvalidCommand:
        return "UNKNOWN_COMMAND";

    case ParseError::InvalidArgument:
        return "INVALID_ARGUMENT";

    case ParseError::TooManyArguments:
        return "TOO_MANY_ARGUMENTS";

    case ParseError::ArgumentTooLong:
        return "ARGUMENT_TOO_LONG";

    case ParseError::None:
        break;
    }

    return "UNKNOWN_ERROR";
}

static const char *managerErrorToString(TestManagerError error) {
    switch (error) {
    case TestManagerError::NONE:
        return "NONE";

    case TestManagerError::TEST_NOT_FOUND:
        return "TEST_NOT_FOUND";

    case TestManagerError::TEST_ALREADY_RUNNING:
        return "TEST_ALREADY_RUNNING";

    case TestManagerError::TEST_START_FAILED:
        return "TEST_START_FAILED";

    case TestManagerError::INVALID_TEST_STATE:
        return "INVALID_TEST_STATE";
    }

    return "UNKNOWN_ERROR";
}

// --------------------------------------------------
// System command handler
// --------------------------------------------------

static void systemCommandHandler(const Command &command, void *context) {
    auto *uart = static_cast<UartLink *>(context);

    if (uart == nullptr) {
        return;
    }

    switch (command.type) {

    case CommandType::Ping:
        uart->writeLine("PONG");
        break;

    case CommandType::Help:
        uart->writeLine("COMMANDS PING HELP INFO STATUS RUN <TEST_ID> STOP TEST_STATUS");
        break;

    case CommandType::Info:
        uart->writeLine("INFO MCU=ESP32-S3 FW=0.1.0");
        break;

    case CommandType::Status:
        uart->writeLine("STATUS READY");
        break;

    default:
        uart->writeLine("ERROR INVALID_SYSTEM_COMMAND");
        break;
    }
}

// --------------------------------------------------
// Test command handler
// --------------------------------------------------

static void testCommandHandler(const Command &command, void *context) {
    auto *handler = static_cast<TestHandlerContext *>(context);

    if (handler == nullptr || handler->uart == nullptr || handler->manager == nullptr) {
        return;
    }

    UartLink &uart = *handler->uart;

    TestManager &manager = *handler->manager;

    switch (command.type) {

        // ----------------------------------------------
        // RUN <TEST_ID>
        // ----------------------------------------------

    case CommandType::Run: {

        const char *testId = command.argv[0];

        if (!manager.startTest(testId)) {

            char response[128];

            std::snprintf(response, sizeof(response), "ERROR %s", managerErrorToString(manager.lastError()));

            uart.writeLine(response);

            break;
        }

        handler->resultPending = true;

        char response[128];

        std::snprintf(response, sizeof(response), "TEST_START OK ID=%s STATE=%s", testId, testStateToString(manager.state()));

        uart.writeLine(response);

        break;
    }

        // ----------------------------------------------
        // STOP
        // ----------------------------------------------

    case CommandType::Stop: {

        if (!manager.stopTest()) {
            uart.writeLine("ERROR NO_ACTIVE_TEST");
            break;
        }

        uart.writeLine("TEST_STOP OK");

        break;
    }

        // ----------------------------------------------
        // TEST_STATUS
        // ----------------------------------------------

    case CommandType::TestStatus: {

        const ITest *test = manager.lastTest();

        if (test == nullptr) {
            uart.writeLine("TEST_STATUS IDLE");
            break;
        }

        char response[128];

        std::snprintf(response, sizeof(response), "TEST_STATUS ID=%s STATE=%s", test->id(), testStateToString(manager.state()));

        uart.writeLine(response);

        break;
    }

    default:
        uart.writeLine("ERROR INVALID_TEST_COMMAND");
        break;
    }
}

// --------------------------------------------------
// Report completed test
// --------------------------------------------------

static void reportTestResult(TestHandlerContext &handler) {

    if (!handler.resultPending) {
        return;
    }

    TestManager &manager = *handler.manager;

    if (manager.isRunning()) {
        return;
    }

    const ITest *test = manager.lastTest();

    const TestResult *result = manager.lastResult();

    if (test == nullptr || result == nullptr) {
        return;
    }

    const TestState state = manager.state();

    if (!isTestFinished(state)) {
        return;
    }

    char response[256];

    std::snprintf(response, sizeof(response), "TEST_RESULT ID=%s STATE=%s SUCCESS=%d ERROR=%s MESSAGE=%s", test->id(),
                  testStateToString(state), result->success ? 1 : 0, testErrorToString(result->errorCode), result->message);

    handler.uart->writeLine(response);

    handler.resultPending = false;
}

// --------------------------------------------------
// Application entry
// --------------------------------------------------

extern "C" void app_main(void) {

    // ----------------------------------------------
    // Hardware objects
    // ----------------------------------------------

    static UartLink uart;

    static GpioTest gpioTest;

    static TestRegistry registry;

    static TestManager testManager(registry);

    static Dispatcher dispatcher;

    // ----------------------------------------------
    // UART initialization
    // ----------------------------------------------

    UartLink::Config uartConfig;

    uartConfig.port = UART_NUM_0;

    uartConfig.tx_pin = GPIO_NUM_43;

    uartConfig.rx_pin = GPIO_NUM_44;

    uartConfig.baudrate = 115200;

    uartConfig.max_line_length = 256;

    ESP_ERROR_CHECK(uart.init(uartConfig));

    ESP_ERROR_CHECK(uart.writeLine("READY CTB_ESP32 PROTOCOL=1"));

    // ----------------------------------------------
    // Test registration
    // ----------------------------------------------

    if (!registry.registerTest(&gpioTest)) {
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    ESP_LOGI("APP", "Registered tests: %u", static_cast<unsigned>(registry.size()));

    // ----------------------------------------------
    // Dispatcher initialization
    // ----------------------------------------------

    TestHandlerContext testContext;

    testContext.uart = &uart;

    testContext.manager = &testManager;

    dispatcher.setSystemHandler(systemCommandHandler, &uart);

    dispatcher.setTestHandler(testCommandHandler, &testContext);

    ESP_LOGI("APP", "Application initialized");

    // ----------------------------------------------
    // Main loop
    // ----------------------------------------------

    while (true) {

        // Process asynchronous hardware test.
        testManager.process();

        // Report result when a test finishes.
        reportTestResult(testContext);

        // Receive UART command.
        std::string line;

        const esp_err_t err = uart.readLine(line, 10);

        if (err == ESP_ERR_TIMEOUT) {
            continue;
        }

        if (err != ESP_OK) {

            uart.writeLine("ERROR UART_RX");

            continue;
        }

        // Parse command.
        const ParseResult parseResult = Parser::parse(line.c_str());

        if (!parseResult.success) {

            std::string response = "ERROR ";

            response += parseErrorToString(parseResult.error);

            uart.writeLine(response);

            continue;
        }

        // Dispatch command.
        const DispatchResult dispatchResult = dispatcher.dispatch(parseResult.command);

        if (!dispatchResult.ok()) {

            switch (dispatchResult.status) {

            case DispatchStatus::UnknownCommand:
                uart.writeLine("ERROR UNKNOWN_COMMAND");
                break;

            case DispatchStatus::InvalidArguments:
                uart.writeLine("ERROR INVALID_ARGUMENTS");
                break;

            case DispatchStatus::HandlerNotRegistered:
                uart.writeLine("ERROR HANDLER_NOT_REGISTERED");
                break;

            case DispatchStatus::Dispatched:
                break;
            }
        }
    }
}