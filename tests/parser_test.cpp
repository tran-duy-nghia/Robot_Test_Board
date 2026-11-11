#include <cassert>
#include <cstring>
#include <iostream>
#include "parser.hpp"

int main() {
    {
        const ParseResult result = Parser::parse("PING");
        assert(result.success);
        assert(result.command.type == CommandType::Ping);
        assert(result.command.argc == 0);
    }

    {
        const ParseResult result = Parser::parse("  run   GPIO_LOOPBACK  ");
        assert(result.success);
        assert(result.command.type == CommandType::Run);
        assert(result.command.argc == 1);
        assert(std::strcmp(result.command.argv[0], "GPIO_LOOPBACK") == 0);
    }

    {
        const ParseResult result = Parser::parse("RUN UART_LOOPBACK 115200");
        assert(!result.success);
        assert(result.error == ParseError::InvalidArgument);
    }

    {
        const ParseResult result = Parser::parse("GPIO_READ 1");
        assert(!result.success);
        assert(result.error == ParseError::InvalidCommand);
    }

    {
        const ParseResult result = Parser::parse("PING extra");
        assert(!result.success);
        assert(result.error == ParseError::InvalidArgument);
    }

    {
        const ParseResult result = Parser::parse("RUN");
        assert(!result.success);
        assert(result.error == ParseError::InvalidArgument);
    }

    {
        const ParseResult result = Parser::parse("RUN A B C D E");
        assert(!result.success);
        assert(result.error == ParseError::TooManyArguments);
    }

    {
        const ParseResult result = Parser::parse("   \t  ");
        assert(!result.success);
        assert(result.error == ParseError::EmptyInput);
    }

    {
        const ParseResult result = Parser::parse(nullptr);
        assert(!result.success);
        assert(result.error == ParseError::EmptyInput);
    }

    {
        const ParseResult result = Parser::parse("RUN ABCDEFGHIJKLMNOPQRSTUVWXYZ123456");
        assert(!result.success);
        assert(result.error == ParseError::ArgumentTooLong);
    }
    
    std::cout << "All tests passed!" << std::endl;

    return 0;
}
