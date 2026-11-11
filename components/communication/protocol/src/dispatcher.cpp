#include "dispatcher.hpp"

void Dispatcher::setSystemHandler(Handler handler, void* context) noexcept {
    m_systemHandler = handler;
    m_systemContext = context;
}

void Dispatcher::setTestHandler(Handler handler, void* context) noexcept {
    m_testHandler = handler;
    m_testContext = context;
}

bool Dispatcher::resolveRoute(const CommandType type, Route& route) noexcept {
    switch (type) {
    case CommandType::Ping:
    case CommandType::Help:
    case CommandType::Info:
    case CommandType::ListTestID:
    case CommandType::Status:
        route = {CommandDomain::System, 0U, 0U};
        return true;

    case CommandType::Run:
        route = {CommandDomain::Test, 1U, 1U};
        return true;

    case CommandType::Stop:
    case CommandType::TestStatus:
        route = {CommandDomain::Test, 0U, 0U};
        return true;

    case CommandType::Unknown:
    default:
        return false;
    }
}

DispatchResult Dispatcher::dispatch(const Command& command) const {
    Route route{};
    if (!resolveRoute(command.type, route)) {
        return {DispatchStatus::UnknownCommand, CommandDomain::None};
    }

    if (command.argc < route.minArguments ||
        command.argc > route.maxArguments ||
        command.argc > MAX_ARGUMENTS) {
        return {DispatchStatus::InvalidArguments, route.domain};
    }

    Handler handler = nullptr;
    void* context = nullptr;

    switch (route.domain) {
    case CommandDomain::System:
        handler = m_systemHandler;
        context = m_systemContext;
        break;

    case CommandDomain::Test:
        handler = m_testHandler;
        context = m_testContext;
        break;

    case CommandDomain::None:
    default:
        return {DispatchStatus::UnknownCommand, CommandDomain::None};
    }

    if (handler == nullptr) {
        return {DispatchStatus::HandlerNotRegistered, route.domain};
    }

    handler(command, context);
    return {DispatchStatus::Dispatched, route.domain};
}

