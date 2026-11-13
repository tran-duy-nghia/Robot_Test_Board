#pragma once

#include "m288t.hpp"
#include "test_interface.hpp"

class M288TTest final : public ITest {
public:
    struct Config {
        M288T::Config motor{};
    };

    M288TTest() = default;
    explicit M288TTest(const Config &config);
    ~M288TTest() override;

    void configure(const Config &config);

    const char *id() const override;
    const char *name() const override;

    bool start() override;
    void stop() override;
    void process() override;

    TestState state() const override;
    const TestResult &result() const override;

private:
    void finish(TestState finalState);
    void fail(TestError error, const char *message);

private:
    Config m_config{};
    M288T m_motor{M288T::Config{}};
};
