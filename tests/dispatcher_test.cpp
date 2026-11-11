#include "dispatcher.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

struct HandlerState {
    std::uint32_t calls{0};
    CommandType lastCommand{CommandType::Unknown};
};

void countHandler(const Command& command, void* context) {
    auto* state = static_cast<HandlerState*>(context);
    ++state->calls;
    state->lastCommand = command.type;
}

Command makeCommand(CommandType type, std::uint8_t argc = 0) {
    Command command{};
    command.type = type;
    command.argc = argc;
    return command;
}

void testSystemRouting() {
    Dispatcher dispatcher;
    HandlerState system{};

    dispatcher.setSystemHandler(countHandler, &system);

    const auto result = dispatcher.dispatch(makeCommand(CommandType::Ping));

    assert(result.status == DispatchStatus::Dispatched);
    assert(result.domain == CommandDomain::System);
    assert(system.calls == 1);
    assert(system.lastCommand == CommandType::Ping);
}

void testTestRouting() {
    Dispatcher dispatcher;
    HandlerState test{};

    dispatcher.setTestHandler(countHandler, &test);

    const auto result = dispatcher.dispatch(makeCommand(CommandType::Run, 1));

    assert(result.status == DispatchStatus::Dispatched);
    assert(result.domain == CommandDomain::Test);
    assert(test.calls == 1);
    assert(test.lastCommand == CommandType::Run);
}

void testInvalidArgumentCount() {
    Dispatcher dispatcher;
    HandlerState system{};
    HandlerState test{};

    dispatcher.setSystemHandler(countHandler, &system);
    dispatcher.setTestHandler(countHandler, &test);

    const auto runWithoutId = dispatcher.dispatch(makeCommand(CommandType::Run));
    const auto pingWithArgument = dispatcher.dispatch(makeCommand(CommandType::Ping, 1));
    const auto stopWithArgument = dispatcher.dispatch(makeCommand(CommandType::Stop, 1));

    assert(runWithoutId.status == DispatchStatus::InvalidArguments);
    assert(pingWithArgument.status == DispatchStatus::InvalidArguments);
    assert(stopWithArgument.status == DispatchStatus::InvalidArguments);
    assert(system.calls == 0);
    assert(test.calls == 0);
}

void testUnknownCommand() {
    Dispatcher dispatcher;
    const auto result = dispatcher.dispatch(makeCommand(CommandType::Unknown));

    assert(result.status == DispatchStatus::UnknownCommand);
    assert(result.domain == CommandDomain::None);
}

void testMissingHandler() {
    Dispatcher dispatcher;
    const auto result = dispatcher.dispatch(makeCommand(CommandType::Info));

    assert(result.status == DispatchStatus::HandlerNotRegistered);
    assert(result.domain == CommandDomain::System);
}

} // namespace

int main() {
    testSystemRouting();
    testTestRouting();
    testInvalidArgumentCount();
    testUnknownCommand();
    testMissingHandler();

    std::cout << "All dispatcher tests passed.\n";
    return 0;
}
