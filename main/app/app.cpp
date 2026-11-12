#include "app.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if ENCODER_TEST_SIGNAL
#include "soc/gpio_struct.h"
#endif
#include <cstdio>
#include <string>

namespace {
constexpr char TAG[] = "APP";
constexpr uint32_t APP_TASK_STACK_SIZE = 4096;
constexpr UBaseType_t APP_TASK_PRIORITY = 5;
constexpr TickType_t APP_LOOP_DELAY = pdMS_TO_TICKS(1);
} // namespace

void App::main() {
    // Static lifetime is required because the application task keeps using
    // this instance after this function returns.
    static App app;
    ESP_ERROR_CHECK(app.initialize());
}

esp_err_t App::initialize() {
    if (m_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    UartLink::Config uartConfig{};
    uartConfig.port = UART_NUM_0;
    uartConfig.tx_pin = GPIO_NUM_43;
    uartConfig.rx_pin = GPIO_NUM_44;
    uartConfig.baudrate = 115200;
    uartConfig.max_line_length = 256;

    ESP_RETURN_ON_ERROR(m_uart.init(uartConfig), TAG, "UART initialization failed");
    ESP_RETURN_ON_ERROR(m_uart.writeLine("READY CTB_ESP32 PROTOCOL=1"), TAG, "UART ready message failed");

    ESP_RETURN_ON_ERROR(this->registerI2CTest(), TAG, "Failed to register I2C test");
    ESP_RETURN_ON_ERROR(this->registerEncoderTest(), TAG, "Failed to register Encoder test");

    m_dispatcher.setSystemHandler(&App::systemCommandHandler, this);
    m_dispatcher.setTestHandler(&App::testCommandHandler, this);

    m_initialized = true;

    const BaseType_t taskCreated = xTaskCreate(&App::taskEntry, "app_task", APP_TASK_STACK_SIZE, this, APP_TASK_PRIORITY, nullptr);

    if (taskCreated != pdPASS) {
        m_initialized = false;
        m_uart.deinit();
        ESP_LOGE(TAG, "Failed to create application task");
        return ESP_ERR_NO_MEM;
    }

#if ENCODER_TEST_SIGNAL
    const BaseType_t sigGenTaskCreated =
        xTaskCreate(&App::taskSignalGen, "sig_gen_task", APP_TASK_STACK_SIZE, this, APP_TASK_PRIORITY, nullptr);
    if (sigGenTaskCreated != pdPASS) {
        ESP_LOGE(TAG, "Failed to create signal generation task");
        return ESP_ERR_NO_MEM;
    }
#endif

    ESP_LOGI(TAG, "Application initialized; registered tests: %u", static_cast<unsigned>(m_registry.size()));

    return ESP_OK;
}

void App::taskEntry(void *argument) {
    auto *app = static_cast<App *>(argument);
    if (app != nullptr) {
        app->run();
    }

    vTaskDelete(nullptr);
}

#if ENCODER_TEST_SIGNAL
void App::taskSignalGen(void *argument) {
    gpio_config_t io_conf = {
        .pin_bit_mask = ENCODER_TEST_PIN_MASK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&io_conf);
    GPIO.out_w1tc = ENCODER_TEST_PIN_MASK;

    while (true) {
        GPIO.out_w1ts = ENCODER_TEST_PIN_MASK; 
        vTaskDelay(pdMS_TO_TICKS(50));
        GPIO.out_w1tc = ENCODER_TEST_PIN_MASK; 
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
#endif

void App::run() {
    while (true) {
        // Advance asynchronous tests and report terminal results.
        m_testManager.process();
        reportTestResult();

        std::string line;
        const esp_err_t err = m_uart.readLine(line, 10);

        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        if (err == ESP_ERR_TIMEOUT) {
            continue;
        }

        if (err != ESP_OK) {
            m_uart.writeLine(err == ESP_ERR_INVALID_SIZE ? "ERROR COMMAND_TOO_LONG" : "ERROR UART_RX");
            continue;
        }

        const ParseResult parseResult = Parser::parse(line.c_str());
        if (!parseResult.success) {
            std::string response{"ERROR "};
            response += parseErrorToString(parseResult.error);
            m_uart.writeLine(response);
            continue;
        }

        const DispatchResult dispatchResult = m_dispatcher.dispatch(parseResult.command);
        if (dispatchResult.ok()) {
            continue;
        }

        switch (dispatchResult.status) {
        case DispatchStatus::UnknownCommand:
            m_uart.writeLine("ERROR UNKNOWN_COMMAND");
            break;

        case DispatchStatus::InvalidArguments:
            m_uart.writeLine("ERROR INVALID_ARGUMENTS");
            break;

        case DispatchStatus::HandlerNotRegistered:
            m_uart.writeLine("ERROR HANDLER_NOT_REGISTERED");
            break;

        case DispatchStatus::Dispatched:
            break;
        }

        vTaskDelay(APP_LOOP_DELAY);
    }
}

void App::systemCommandHandler(const Command &command, void *context) {
    auto *app = static_cast<App *>(context);
    if (app == nullptr) {
        return;
    }

    switch (command.type) {
    case CommandType::Ping: {
        app->m_uart.writeLine("[PONG]");
        break;
    }

    case CommandType::Help: {
        app->m_uart.writeLine("[COMMANDS]: PING HELP INFO STATUS RUN <TEST_ID> STOP TEST_STATUS LIST_TEST_ID");
        break;
    }

    case CommandType::Info: {
        app->m_uart.writeLine("[INFO]: MCU=ESP32-S3 FW=0.1.0");
        break;
    }

    case CommandType::Status: {
        app->m_uart.writeLine("[STATUS]: READY");
        break;
    }

    case CommandType::ListTestID: {
        std::string response = "[TEST_LIST]: ";
        for (std::size_t i = 0; i < app->m_registry.size(); ++i) {
            const ITest *test = app->m_registry.getAt(i);
            if (test != nullptr) {
                response += " ";
                response += test->id();
            }
        }
        app->m_uart.writeLine(response);
        break;
    }

    default:
        app->m_uart.writeLine("[ERROR]: INVALID_SYSTEM_COMMAND");
        break;
    }
}

void App::testCommandHandler(const Command &command, void *context) {
    auto *app = static_cast<App *>(context);
    if (app == nullptr) {
        return;
    }

    switch (command.type) {
    case CommandType::Run: {
        const char *testId = command.argv[0];
        if (!app->m_testManager.startTest(testId)) {
            char response[128];
            std::snprintf(response, sizeof(response), "[ERROR]: %s", managerErrorToString(app->m_testManager.lastError()));
            app->m_uart.writeLine(response);
            break;
        }

        app->m_resultPending = true;

        char response[128];
        std::snprintf(response, sizeof(response), "[TEST_START]: OK ID=%s STATE=%s", testId, testStateToString(app->m_testManager.state()));
        app->m_uart.writeLine(response);
        break;
    }

    case CommandType::Stop:
        if (!app->m_testManager.stopTest()) {
            app->m_uart.writeLine("[ERROR]: NO_ACTIVE_TEST");
        } else {
            app->m_uart.writeLine("[TEST_STOP]: OK");
        }
        break;

    case CommandType::TestStatus: {
        const ITest *test = app->m_testManager.lastTest();
        if (test == nullptr) {
            app->m_uart.writeLine("[TEST_STATUS]: IDLE");
            break;
        }

        char response[128];
        std::snprintf(response, sizeof(response), "[TEST_STATUS]: ID=%s STATE=%s", test->id(),
                      testStateToString(app->m_testManager.state()));
        app->m_uart.writeLine(response);
        break;
    }

    default:
        app->m_uart.writeLine("[ERROR]: INVALID_TEST_COMMAND");
        break;
    }
}

void App::reportTestResult() {
    if (!m_resultPending || m_testManager.isRunning()) {
        return;
    }

    const ITest *test = m_testManager.lastTest();
    const TestResult *result = m_testManager.lastResult();

    if (test == nullptr || result == nullptr) {
        return;
    }

    const TestState state = m_testManager.state();
    if (!isTestFinished(state)) {
        return;
    }

    char response[256];
    std::snprintf(response, sizeof(response), "[TEST_RESULT]: ID=%s STATE=%s SUCCESS=%d ERROR=%s MESSAGE=%s", test->id(),
                  testStateToString(state), result->success ? 1 : 0, testErrorToString(result->errorCode), result->message);

    m_uart.writeLine(response);
    m_resultPending = false;
}

const char *App::parseErrorToString(ParseError error) {
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

const char *App::managerErrorToString(TestManagerError error) {
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

esp_err_t App::registerI2CTest() {
    I2CTest::Config i2cConfig{};
    i2cConfig.port = I2C_TEST_PORT;
    i2cConfig.sda_pin = I2C_TEST_SDA_PIN;
    i2cConfig.scl_pin = I2C_TEST_SCL_PIN;
    i2cConfig.frequency_hz = I2C_TEST_FREQUENCY_HZ;
    i2cConfig.probe_timeout_ms = I2C_TEST_PROBE_TIMEOUT_MS;
    i2cConfig.interval_ms = I2C_TEST_INTERVAL_MS;
    i2cConfig.enable_internal_pullup = true;

    m_i2cTest = I2CTest(i2cConfig);

    if (!m_registry.registerTest(&m_i2cTest)) {
        ESP_LOGE(TAG, "Failed to register I2C test");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t App::registerEncoderTest() {
    EncoderTest::Config config{};
    config.ch1 = ENCODER_TEST_CH1_PIN;
    config.ch2 = ENCODER_TEST_CH2_PIN;
    config.ch3 = ENCODER_TEST_CH3_PIN;

    m_encoderTest = EncoderTest(config);

    if (!m_registry.registerTest(&m_encoderTest)) {
        ESP_LOGE(TAG, "Failed to register Encoder test");
        return ESP_FAIL;
    }

    return ESP_OK;
}
