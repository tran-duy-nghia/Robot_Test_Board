#pragma once

#include "test_state.hpp"
#include "test_result.hpp"

class ITest {
public:
    virtual ~ITest() = default;

    /**
     * @brief Unique test identifier.
     */
    virtual const char* id() const = 0;

    /**
     * @brief Human-readable test name.
     */
    virtual const char* name() const = 0;

    /**
     * @brief Initialize and start the test.
     *
     * @return true if the test starts successfully.
     */
    virtual bool start() = 0;

    /**
     * @brief Stop the running test.
     */
    virtual void stop() = 0;

    /**
     * @brief Process one test iteration.
     *
     * Called periodically by TestManager.
     * Must not block for a long time.
     */
    virtual void process() = 0;

    /**
     * @brief Current test state.
     */
    virtual TestState state() const = 0;

    /**
     * @brief Get the latest test result.
     */
    virtual const TestResult& result() const = 0;
};
