#pragma once

#include <cstdint>
#include <command.hpp> 

enum class DispatchStatus : std::uint8_t {
    Dispatched,
    UnknownCommand,
    InvalidArguments,
    HandlerNotRegistered
};

enum class CommandDomain : std::uint8_t {
    None,
    System,
    Test
};

struct DispatchResult {
    DispatchStatus status{DispatchStatus::UnknownCommand};
    CommandDomain domain{CommandDomain::None};

    constexpr bool ok() const noexcept {
        return status == DispatchStatus::Dispatched;
    }
};

class Dispatcher {
public:
    using Handler = void (*)(const Command& command, void* context);

    Dispatcher() = default;

    void setSystemHandler(Handler handler, void* context = nullptr) noexcept;
    void setTestHandler(Handler handler, void* context = nullptr) noexcept;

    DispatchResult dispatch(const Command& command) const;

private:
    struct Route {
        CommandDomain domain;
        std::uint8_t minArguments;
        std::uint8_t maxArguments;
    };

    static bool resolveRoute(CommandType type, Route& route) noexcept;

    Handler m_systemHandler{nullptr};
    void* m_systemContext{nullptr};

    Handler m_testHandler{nullptr};
    void* m_testContext{nullptr};
};
