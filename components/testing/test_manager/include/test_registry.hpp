#pragma once

#include <array>
#include <cstddef>

#include "test_interface.hpp"

constexpr std::size_t MAX_REGISTERED_TESTS = 16;

class TestRegistry {
public:
    TestRegistry() = default;

    ~TestRegistry() = default;

    TestRegistry(const TestRegistry &) = delete;

    TestRegistry &operator=(const TestRegistry &) = delete;

    /**
     * @brief Register a test instance.
     *
     * The registry does not take ownership of the test.
     *
     * @param test Pointer to test instance.
     * @return true if registration succeeds.
     */
    bool registerTest(ITest *test);

    /**
     * @brief Unregister a test by ID.
     *
     * @param id Unique test identifier.
     * @return true if the test was found and removed.
     */
    bool unregisterTest(const char *id);

    /**
     * @brief Find a test by ID.
     *
     * @param id Test identifier.
     * @return Pointer to test or nullptr.
     */
    ITest *findById(const char *id);

    const ITest *findById(const char *id) const;

    /**
     * @brief Get test by index.
     *
     * @param index Position in registry.
     * @return Pointer to test or nullptr.
     */
    ITest *getAt(std::size_t index);

    const ITest *getAt(std::size_t index) const;

    /**
     * @brief Number of registered tests.
     */
    std::size_t size() const;

    /**
     * @brief Check whether registry is empty.
     */
    bool isEmpty() const;

    /**
     * @brief Check whether registry is full.
     */
    bool isFull() const;

    /**
     * @brief Remove all registered test references.
     *
     * Does not destroy test instances.
     */
    void clear();

private:
    std::array<ITest *, MAX_REGISTERED_TESTS> m_tests{};

    std::size_t m_size = 0;
};