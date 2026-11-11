#pragma once

#include "test_registry.hpp"

enum class TestManagerError {
    NONE = 0,
    TEST_NOT_FOUND,
    TEST_ALREADY_RUNNING,
    TEST_START_FAILED,
    INVALID_TEST_STATE
};

class TestManager {
public:
    explicit TestManager(TestRegistry& registry);

    ~TestManager() = default;

    TestManager(const TestManager&) = delete;
    TestManager& operator=(const TestManager&) = delete;

    /**
     * @brief Start a test by its registered ID.
     *
     * @param id Unique test identifier.
     * @return true if the test starts successfully.
     */
    bool startTest(const char* id);

    /**
     * @brief Stop the currently running test.
     *
     * @return true if a running test was stopped.
     */
    bool stopTest();

    /**
     * @brief Process the currently active test.
     *
     * Must be called periodically by the application.
     */
    void process();

    /**
     * @brief Check whether a test is currently running.
     */
    bool isRunning() const;

    /**
     * @brief Get current test state.
     */
    TestState state() const;

    /**
     * @brief Get the currently active test.
     */
    ITest* activeTest();

    const ITest* activeTest() const;

    /**
     * @brief Get the most recently executed test.
     */
    ITest* lastTest();

    const ITest* lastTest() const;

    /**
     * @brief Get the latest test result.
     */
    const TestResult* lastResult() const;

    /**
     * @brief Get the last manager error.
     */
    TestManagerError lastError() const;

    /**
     * @brief Reset manager error.
     */
    void clearError();

private:
    void setError(TestManagerError error);

    void finalizeActiveTest();

private:
    TestRegistry& m_registry;

    ITest* m_activeTest = nullptr;

    ITest* m_lastTest = nullptr;

    TestManagerError m_lastError = TestManagerError::NONE;
};
