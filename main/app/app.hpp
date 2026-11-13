#pragma once

#include "esp_err.h"

#include "dispatcher.hpp"
#include "encoder_test.hpp"
#include "i2c_test.hpp"
#include "uart_test.hpp"
#include "pwm_test.hpp"
#include "parser.hpp"
#include "test_manager.hpp"
#include "test_registry.hpp"
#include "uart_link.hpp"

class App {
public:
    App() = default;
    ~App() = default;

    App(const App &) = delete;
    App &operator=(const App &) = delete;

    /**
     * @brief Application entry called by ESP-IDF app_main().
     *
     * Initializes the application and starts its worker task.
     */
    static void main();

private:
    esp_err_t initialize();

    static void taskEntry(void *argument);

    void run();

    static void systemCommandHandler(const Command &command, void *context);
    static void testCommandHandler(const Command &command, void *context);

    void reportTestResult();

    static const char *parseErrorToString(ParseError error);
    static const char *managerErrorToString(TestManagerError error);

    esp_err_t registerI2CTest();
    esp_err_t registerEncoderTest();
    esp_err_t registerUartTest();
    esp_err_t registerPwmTest();

private:
    UartLink m_uart{};

    I2CTest m_i2cTest{};
    EncoderTest m_encoderTest{};
    UartTest m_uartTest{};
    PwmTest m_pwmTest{};

    TestRegistry m_registry{};
    TestManager m_testManager{m_registry};
    Dispatcher m_dispatcher{};

    bool m_resultPending{false};
    bool m_initialized{false};
};
