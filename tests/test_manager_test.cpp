#include <cassert>
#include <cstdio>

#include "test_manager.hpp"

// --------------------------------------------------
// Mock Test
// --------------------------------------------------

class MockTest : public ITest {
public:
    explicit MockTest(const char* testId, int processCount = 3)
        : m_id(testId),
          m_requiredProcessCount(processCount) {
    }

    const char* id() const override {
        return m_id;
    }

    const char* name() const override {
        return "Mock Test";
    }

    bool start() override {
        if (m_state == TestState::RUNNING) {
            return false;
        }

        m_state = TestState::RUNNING;
        m_processCount = 0;
        m_result.reset();

        return true;
    }

    void stop() override {
        if (m_state != TestState::RUNNING) {
            return;
        }

        m_state = TestState::STOPPED;

        m_result.setFailure(
            TestError::INTERNAL_ERROR,
            "Test stopped by user"
        );
    }

    void process() override {
        if (m_state != TestState::RUNNING) {
            return;
        }

        ++m_processCount;

        if (m_processCount >= m_requiredProcessCount) {
            m_state = TestState::PASSED;

            m_result.setSuccess("Mock test passed");
        }
    }

    TestState state() const override {
        return m_state;
    }

    const TestResult& result() const override {
        return m_result;
    }

private:
    const char* m_id;

    int m_requiredProcessCount = 3;
    int m_processCount = 0;

    TestState m_state = TestState::IDLE;

    TestResult m_result{};
};

// --------------------------------------------------
// Tests
// --------------------------------------------------

void testStartTest() {
    TestRegistry registry;

    MockTest gpio("GPIO");

    registry.registerTest(&gpio);

    TestManager manager(registry);

    assert(manager.startTest("GPIO"));

    assert(manager.isRunning());

    assert(manager.state() == TestState::RUNNING);

    assert(manager.activeTest() == &gpio);

    std::printf("[PASS] Start test\n");
}

void testTestNotFound() {
    TestRegistry registry;

    TestManager manager(registry);

    assert(!manager.startTest("UNKNOWN"));

    assert(manager.lastError() ==
           TestManagerError::TEST_NOT_FOUND);

    assert(!manager.isRunning());

    std::printf("[PASS] Test not found\n");
}

void testMultipleTests() {
    TestRegistry registry;

    MockTest gpio("GPIO");
    MockTest i2c("I2C");

    registry.registerTest(&gpio);
    registry.registerTest(&i2c);

    TestManager manager(registry);

    assert(manager.startTest("GPIO"));

    // Cannot start I2C while GPIO is running.
    assert(!manager.startTest("I2C"));

    assert(manager.lastError() ==
           TestManagerError::TEST_ALREADY_RUNNING);

    assert(manager.activeTest() == &gpio);

    assert(i2c.state() == TestState::IDLE);

    std::printf("[PASS] Prevent multiple active tests\n");
}

void testProcessUntilFinished() {
    TestRegistry registry;

    MockTest gpio("GPIO", 3);

    registry.registerTest(&gpio);

    TestManager manager(registry);

    assert(manager.startTest("GPIO"));

    manager.process();
    assert(manager.isRunning());

    manager.process();
    assert(manager.isRunning());

    manager.process();

    assert(!manager.isRunning());

    assert(manager.activeTest() == nullptr);

    assert(manager.state() == TestState::PASSED);

    assert(manager.lastTest() == &gpio);

    const TestResult* result = manager.lastResult();

    assert(result != nullptr);
    assert(result->success);

    std::printf("[PASS] Process until finished\n");
}

void testStopTest() {
    TestRegistry registry;

    MockTest gpio("GPIO");

    registry.registerTest(&gpio);

    TestManager manager(registry);

    assert(manager.startTest("GPIO"));

    assert(manager.stopTest());

    assert(!manager.isRunning());

    assert(manager.state() == TestState::STOPPED);

    assert(manager.activeTest() == nullptr);

    assert(manager.lastTest() == &gpio);

    std::printf("[PASS] Stop test\n");
}

void testRestartTest() {
    TestRegistry registry;

    MockTest gpio("GPIO", 2);

    registry.registerTest(&gpio);

    TestManager manager(registry);

    // First execution.
    assert(manager.startTest("GPIO"));

    manager.process();
    manager.process();

    assert(manager.state() == TestState::PASSED);

    // Second execution.
    assert(manager.startTest("GPIO"));

    assert(manager.state() == TestState::RUNNING);

    manager.process();
    manager.process();

    assert(manager.state() == TestState::PASSED);

    std::printf("[PASS] Restart completed test\n");
}

void testProcessWhenIdle() {
    TestRegistry registry;

    MockTest gpio("GPIO");

    registry.registerTest(&gpio);

    TestManager manager(registry);

    // Calling process without an active test must be safe.
    manager.process();

    assert(manager.state() == TestState::IDLE);

    assert(!manager.isRunning());

    std::printf("[PASS] Process when idle\n");
}

// --------------------------------------------------
// Main
// --------------------------------------------------

int main() {
    testStartTest();
    testTestNotFound();
    testMultipleTests();
    testProcessUntilFinished();
    testStopTest();
    testRestartTest();
    testProcessWhenIdle();

    std::printf("\nAll TestManager tests passed!\n");

    return 0;
}