#pragma once

#include <cstdint>
#include <cstddef>

constexpr size_t MAX_ARGUMENTS = 4;
constexpr size_t MAX_ARGUMENT_LENGTH = 32;

enum class CommandType : uint8_t {
    Ping,
    Help,
    Info,
    Status,
    Run,
    Stop,
    TestStatus,
    Unknown
};

struct Command {
    CommandType type;

    uint8_t argc = 0;

    char argv[MAX_ARGUMENTS][MAX_ARGUMENT_LENGTH]{};
};