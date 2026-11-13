#include "app.hpp"
#include "app_config.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
    ESP_RETURN_ON_ERROR(this->registerUartTest(), TAG, "Failed to register UART test");
    ESP_RETURN_ON_ERROR(this->registerPwmTest(), TAG, "Failed to register PWM test");
    ESP_RETURN_ON_ERROR(this->registerM288TTest(), TAG, "Failed to register M288T test");

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

esp_err_t App::registerUartTest() {
    UartTest::Config config{};
    config.port = UART_TEST_PORT;
    config.tx_pin = UART_TEST_TX_PIN;
    config.rx_pin = UART_TEST_RX_PIN;
    config.baudrate = UART_TEST_BAUDRATE;
    config.timeout_ms = UART_TEST_TIMEOUT_MS;
    config.expected_bytes = UART_TEST_EXPECTED_BYTES;

    m_uartTest = UartTest(config);

    if (!m_registry.registerTest(&m_uartTest)) {
        ESP_LOGE(TAG, "Failed to register UART test");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t App::registerPwmTest() {
    PwmTest::Config config{};
    config.pins = {PWM_TEST_CH1_PIN, PWM_TEST_CH2_PIN, PWM_TEST_CH3_PIN, PWM_TEST_CH4_PIN};
    config.frequency_hz = PWM_TEST_FREQUENCY_HZ;
    config.resolution_bits = PWM_TEST_RESOLUTION_BITS;
    config.duty_percent = PWM_TEST_DUTY_PERCENT;
    config.channel_duration_ms = PWM_TEST_CHANNEL_DURATION_MS;

    m_pwmTest = PwmTest(config);

    if (!m_registry.registerTest(&m_pwmTest)) {
        ESP_LOGE(TAG, "Failed to register PWM test");
        return ESP_FAIL;
    }

    return ESP_OK;
}


esp_err_t App::registerM288TTest() {
    M288TTest::Config config{};

    config.motor.id = M288T_TEST_DEVICE_ID;
    config.motor.expected_model_number = M288T::MODEL_NUMBER_XL330_M288_T;

    config.motor.transport.tx_pin = M288T_TEST_TX_PIN;
    config.motor.transport.rx_pin = M288T_TEST_RX_PIN;
    config.motor.transport.baudrate = M288T_TEST_BAUDRATE;
    config.motor.transport.response_timeout_ms = M288T_TEST_RESPONSE_TIMEOUT_MS;
    config.motor.transport.tx_direction_level = M288T_TEST_TX_DIRECTION_LEVEL;
    config.motor.transport.rx_direction_level = M288T_TEST_RX_DIRECTION_LEVEL;

    m_m288tTest.configure(config);

    if (!m_registry.registerTest(&m_m288tTest)) {
        ESP_LOGE(TAG, "Failed to register M288T test");
        return ESP_FAIL;
    }

    return ESP_OK;
}
