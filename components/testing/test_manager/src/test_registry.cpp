#include "test_registry.hpp"

#include <cstring>

bool TestRegistry::registerTest(ITest* test) {
    if (test == nullptr) {
        return false;
    }

    if (isFull()) {
        return false;
    }

    const char* testId = test->id();

    if (testId == nullptr || testId[0] == '\0') {
        return false;
    }

    // Prevent duplicate IDs and duplicate instances.
    for (std::size_t i = 0; i < m_size; ++i) {
        if (m_tests[i] == test) {
            return false;
        }

        if (std::strcmp(m_tests[i]->id(), testId) == 0) {
            return false;
        }
    }

    m_tests[m_size] = test;
    ++m_size;

    return true;
}

bool TestRegistry::unregisterTest(const char* id) {
    if (id == nullptr || id[0] == '\0') {
        return false;
    }

    for (std::size_t i = 0; i < m_size; ++i) {
        if (std::strcmp(m_tests[i]->id(), id) == 0) {

            // Compact the array.
            for (std::size_t j = i; j + 1 < m_size; ++j) {
                m_tests[j] = m_tests[j + 1];
            }

            --m_size;
            m_tests[m_size] = nullptr;

            return true;
        }
    }

    return false;
}

ITest* TestRegistry::findById(const char* id) {
    if (id == nullptr || id[0] == '\0') {
        return nullptr;
    }

    for (std::size_t i = 0; i < m_size; ++i) {
        if (std::strcmp(m_tests[i]->id(), id) == 0) {
            return m_tests[i];
        }
    }

    return nullptr;
}

const ITest* TestRegistry::findById(const char* id) const {
    if (id == nullptr || id[0] == '\0') {
        return nullptr;
    }

    for (std::size_t i = 0; i < m_size; ++i) {
        if (std::strcmp(m_tests[i]->id(), id) == 0) {
            return m_tests[i];
        }
    }

    return nullptr;
}

ITest* TestRegistry::getAt(std::size_t index) {
    if (index >= m_size) {
        return nullptr;
    }

    return m_tests[index];
}

const ITest* TestRegistry::getAt(std::size_t index) const {
    if (index >= m_size) {
        return nullptr;
    }

    return m_tests[index];
}

std::size_t TestRegistry::size() const {
    return m_size;
}

bool TestRegistry::isEmpty() const {
    return m_size == 0;
}

bool TestRegistry::isFull() const {
    return m_size >= MAX_REGISTERED_TESTS;
}

void TestRegistry::clear() {
    for (std::size_t i = 0; i < m_size; ++i) {
        m_tests[i] = nullptr;
    }

    m_size = 0;
}

