#pragma once

#include "command.hpp"

enum class ParseError : uint8_t {
    None,
    EmptyInput,
    InvalidCommand,
    InvalidArgument,
    TooManyArguments,
    ArgumentTooLong
};

struct ParseResult {
    bool success = false;
    ParseError error = ParseError::None;
    Command command{};
};

class Parser {
public:
    static ParseResult parse(const char* line);
};