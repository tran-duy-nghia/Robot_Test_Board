#pragma once

#include "esp_err.h"

#include "app_config.hpp"
#include "dispatcher.hpp"
#include "encoder_test.hpp"
#include "i2c_test.hpp"
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

#if ENCODER_TEST_SIGNAL
    static void taskSignalGen(void *argument);
#endif

    void run();

    static void systemCommandHandler(const Command &command, void *context);
    static void testCommandHandler(const Command &command, void *context);

    void reportTestResult();

    static const char *parseErrorToString(ParseError error);
    static const char *managerErrorToString(TestManagerError error);

    esp_err_t registerI2CTest();
    esp_err_t registerEncoderTest();

private:
    UartLink m_uart{};

    I2CTest m_i2cTest{};
    EncoderTest m_encoderTest{};

    TestRegistry m_registry{};
    TestManager m_testManager{m_registry};
    Dispatcher m_dispatcher{};

    bool m_resultPending{false};
    bool m_initialized{false};
};
