#include <cassert>
#include <cstdio>

#include "test_registry.hpp"

class MockTest : public ITest {
public:
    explicit MockTest(const char* testId)
        : m_id(testId) {}

    const char* id() const override {
        return m_id;
    }

    const char* name() const override {
        return "Mock Test";
    }

    bool start() override {
        return true;
    }

    void stop() override {}

    void process() override {}

    TestState state() const override {
        return TestState::IDLE;
    }

    const TestResult& result() const override {
        return m_result;
    }

private:
    const char* m_id;

    TestResult m_result{};
};

void testRegister() {
    TestRegistry registry;

    MockTest gpio("GPIO");
    MockTest i2c("I2C");

    assert(registry.registerTest(&gpio));
    assert(registry.registerTest(&i2c));

    assert(registry.size() == 2);

    std::printf("[PASS] Register test\n");
}

void testDuplicateRegistration() {
    TestRegistry registry;

    MockTest gpio("GPIO");
    MockTest duplicate("GPIO");

    assert(registry.registerTest(&gpio));

    assert(!registry.registerTest(&gpio));
    assert(!registry.registerTest(&duplicate));

    assert(registry.size() == 1);

    std::printf("[PASS] Duplicate registration\n");
}

void testFindById() {
    TestRegistry registry;

    MockTest gpio("GPIO");

    assert(registry.registerTest(&gpio));

    ITest* test = registry.findById("GPIO");

    assert(test != nullptr);
    assert(test == &gpio);

    assert(registry.findById("UNKNOWN") == nullptr);
    assert(registry.findById(nullptr) == nullptr);

    std::printf("[PASS] Find test by ID\n");
}

void testUnregister() {
    TestRegistry registry;

    MockTest gpio("GPIO");
    MockTest i2c("I2C");

    assert(registry.registerTest(&gpio));
    assert(registry.registerTest(&i2c));

    assert(registry.unregisterTest("GPIO"));

    assert(registry.size() == 1);
    assert(registry.findById("GPIO") == nullptr);
    assert(registry.findById("I2C") == &i2c);

    assert(!registry.unregisterTest("UNKNOWN"));

    std::printf("[PASS] Unregister test\n");
}

void testGetAt() {
    TestRegistry registry;

    MockTest gpio("GPIO");
    MockTest i2c("I2C");

    assert(registry.registerTest(&gpio));
    assert(registry.registerTest(&i2c));

    assert(registry.getAt(0) == &gpio);
    assert(registry.getAt(1) == &i2c);
    assert(registry.getAt(2) == nullptr);

    std::printf("[PASS] Get test by index\n");
}

void testClear() {
    TestRegistry registry;

    MockTest gpio("GPIO");
    MockTest i2c("I2C");

    assert(registry.registerTest(&gpio));
    assert(registry.registerTest(&i2c));

    registry.clear();

    assert(registry.isEmpty());
    assert(registry.size() == 0);
    assert(registry.findById("GPIO") == nullptr);

    std::printf("[PASS] Clear registry\n");
}

void testCapacity() {
    TestRegistry registry;

    MockTest* tests[MAX_REGISTERED_TESTS];

    char ids[MAX_REGISTERED_TESTS][16];

    for (std::size_t i = 0; i < MAX_REGISTERED_TESTS; ++i) {
        std::snprintf(ids[i], sizeof(ids[i]), "TEST_%02zu", i);

        tests[i] = new MockTest(ids[i]);

        assert(registry.registerTest(tests[i]));
    }

    assert(registry.isFull());
    assert(registry.size() == MAX_REGISTERED_TESTS);

    MockTest extra("EXTRA");

    assert(!registry.registerTest(&extra));

    for (auto* test : tests) {
        delete test;
    }

    std::printf("[PASS] Registry capacity\n");
}

int main() {
    testRegister();
    testDuplicateRegistration();
    testFindById();
    testUnregister();
    testGetAt();
    testClear();
    testCapacity();

    std::printf("\nAll TestRegistry tests passed!\n");

    return 0;
}