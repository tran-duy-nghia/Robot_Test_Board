#include "test_manager.hpp"

TestManager::TestManager(TestRegistry& registry)
    : m_registry(registry) {
}

bool TestManager::startTest(const char* id) {
    clearError();

    // Do not allow multiple tests to run simultaneously.
    if (isRunning()) {
        setError(TestManagerError::TEST_ALREADY_RUNNING);
        return false;
    }

    if (id == nullptr || id[0] == '\0') {
        setError(TestManagerError::TEST_NOT_FOUND);
        return false;
    }

    ITest* test = m_registry.findById(id);

    if (test == nullptr) {
        setError(TestManagerError::TEST_NOT_FOUND);
        return false;
    }

    // A test must be in IDLE or a terminal state before restarting.
    const TestState currentState = test->state();

    if (currentState != TestState::IDLE &&
        !isTestFinished(currentState)) {

        setError(TestManagerError::INVALID_TEST_STATE);
        return false;
    }

    // Keep a reference to the latest test.
    m_lastTest = test;

    // Attempt to start the test.
    if (!test->start()) {
        // A failed start must not leave a running test behind.
        if (test->state() == TestState::RUNNING) {
            test->stop();
        }

        setError(TestManagerError::TEST_START_FAILED);
        return false;
    }

    // Validate the state after start().
    const TestState newState = test->state();

    if (newState == TestState::RUNNING) {
        m_activeTest = test;
        return true;
    }

    // Synchronous tests may finish inside start().
    if (isTestFinished(newState)) {
        m_activeTest = nullptr;
        return true;
    }

    setError(TestManagerError::INVALID_TEST_STATE);
    return false;
}

bool TestManager::stopTest() {
    clearError();

    if (!isRunning()) {
        return false;
    }

    m_activeTest->stop();

    if (m_activeTest->state() != TestState::STOPPED) {
        setError(TestManagerError::INVALID_TEST_STATE);
        return false;
    }

    finalizeActiveTest();

    return true;
}

void TestManager::process() {
    if (!isRunning()) {
        return;
    }

    m_activeTest->process();

    // Automatically release the active test when finished.
    if (isTestFinished(m_activeTest->state())) {
        finalizeActiveTest();
    }
}

bool TestManager::isRunning() const {
    return m_activeTest != nullptr &&
           m_activeTest->state() == TestState::RUNNING;
}

TestState TestManager::state() const {
    if (m_activeTest != nullptr) {
        return m_activeTest->state();
    }

    if (m_lastTest != nullptr) {
        return m_lastTest->state();
    }

    return TestState::IDLE;
}

ITest* TestManager::activeTest() {
    return m_activeTest;
}

const ITest* TestManager::activeTest() const {
    return m_activeTest;
}

ITest* TestManager::lastTest() {
    return m_lastTest;
}

const ITest* TestManager::lastTest() const {
    return m_lastTest;
}

const TestResult* TestManager::lastResult() const {
    if (m_lastTest == nullptr) {
        return nullptr;
    }

    return &m_lastTest->result();
}

TestManagerError TestManager::lastError() const {
    return m_lastError;
}

void TestManager::clearError() {
    m_lastError = TestManagerError::NONE;
}

void TestManager::setError(TestManagerError error) {
    m_lastError = error;
}

void TestManager::finalizeActiveTest() {
    m_activeTest = nullptr;
}
