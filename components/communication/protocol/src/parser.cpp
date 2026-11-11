#include "parser.hpp"
#include <cstddef>

namespace {
bool isWhitespace(const char ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; }

char toUpperAscii(const char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return static_cast<char>(ch - ('a' - 'A'));
    }
    return ch;
}

bool equalsIgnoreCase(const char *lhs, const char *rhs) {
    while (*lhs != '\0' && *rhs != '\0') {
        if (toUpperAscii(*lhs) != toUpperAscii(*rhs)) {
            return false;
        }
        ++lhs;
        ++rhs;
    }

    return *lhs == '\0' && *rhs == '\0';
}

CommandType commandTypeFromToken(const char *token) {
    if (equalsIgnoreCase(token, "PING")) {
        return CommandType::Ping;
    }
    if (equalsIgnoreCase(token, "HELP")) {
        return CommandType::Help;
    }
    if (equalsIgnoreCase(token, "INFO")) {
        return CommandType::Info;
    }
    if (equalsIgnoreCase(token, "STATUS")) {
        return CommandType::Status;
    }
    if (equalsIgnoreCase(token, "RUN")) {
        return CommandType::Run;
    }
    if (equalsIgnoreCase(token, "STOP")) {
        return CommandType::Stop;
    }
    if (equalsIgnoreCase(token, "TEST_STATUS")) {
        return CommandType::TestStatus;
    }
    if (equalsIgnoreCase(token, "LIST_TEST_ID")) {
        return CommandType::ListTestID;
    }   

    return CommandType::Unknown;
}

ParseError validateArgumentCount(const Command &command) {
    switch (command.type) {
    case CommandType::Run:
        return command.argc == 1 ? ParseError::None : ParseError::InvalidArgument;

    case CommandType::Ping:
    case CommandType::Help:
    case CommandType::Info:
    case CommandType::Status:
    case CommandType::Stop:
    case CommandType::ListTestID:
    case CommandType::TestStatus:
        return command.argc == 0 ? ParseError::None : ParseError::InvalidArgument;

    case CommandType::Unknown:
        return ParseError::InvalidCommand;
    }

    return ParseError::InvalidCommand;
}
} // namespace

ParseResult Parser::parse(const char *line) {
    ParseResult result{};

    if (line == nullptr) {
        result.error = ParseError::EmptyInput;
        return result;
    }

    // Skip leading whitespace and reject an empty/whitespace-only line.
    while (isWhitespace(*line)) {
        ++line;
    }

    if (*line == '\0') {
        result.error = ParseError::EmptyInput;
        return result;
    }

    // Parse command token and then its arguments into fixed-size storage.
    char commandToken[MAX_ARGUMENT_LENGTH]{};
    std::size_t commandLength = 0;

    while (*line != '\0' && !isWhitespace(*line)) {
        if (commandLength >= sizeof(commandToken) - 1) {
            result.error = ParseError::ArgumentTooLong;
            return result;
        }
        commandToken[commandLength++] = *line++;
    }

    result.command.type = commandTypeFromToken(commandToken);
    if (result.command.type == CommandType::Unknown) {
        result.error = ParseError::InvalidCommand;
        return result;
    }

    while (true) {
        while (isWhitespace(*line)) {
            ++line;
        }

        if (*line == '\0') {
            break;
        }

        if (result.command.argc >= MAX_ARGUMENTS) {
            result.error = ParseError::TooManyArguments;
            return result;
        }

        char *argument = result.command.argv[result.command.argc];
        std::size_t argumentLength = 0;

        while (*line != '\0' && !isWhitespace(*line)) {
            if (argumentLength >= MAX_ARGUMENT_LENGTH - 1) {
                result.error = ParseError::ArgumentTooLong;
                return result;
            }
            argument[argumentLength++] = *line++;
        }

        ++result.command.argc;
    }

    result.error = validateArgumentCount(result.command);
    result.success = result.error == ParseError::None;
    return result;
}