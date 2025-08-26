#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <chrono>          // std::chrono::system_clock::now()
#include <cstdio>          // stderr
#include <format>          // std::format, std::vformat, std::make_format_args
#include <print>           // std::print
#include <source_location> // std::source_location, std::source_location::current
#include <string_view>     // std::string_view

#ifndef NDEBUG
#    define logd(format, ...) logging::log(format, ##__VA_ARGS__)
#else
#    define logd(...) static_cast<void>(0)
#endif

namespace logging
{

template<typename... Args>
struct log
{
    log(std::string_view fmt, Args&&... args, const std::source_location& loc = std::source_location::current())
    {
        std::print(stderr, "[\u001b[1m{}\u001b[0m] [{}\u001b[0m] {}:{}: {}\n", std::chrono::system_clock::now(), "\u001b[1m\u001b[94mDEBUG", loc.file_name(), loc.line(), std::vformat(fmt, std::make_format_args(args...)));
    }

private:
};

template<typename... Args>
log(std::string_view, Args&&... args) -> log<Args...>;

} // namespace logging

#endif