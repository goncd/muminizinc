#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <chrono>          // std::chrono::system_clock::now()
#include <cstdio>          // stdout, stderr
#include <format>          // std::format, std::vformat, std::make_format_args
#include <print>           // std::print
#include <source_location> // std::source_location, std::source_location::current
#include <string_view>     // std::string_view
#include <utility>         // std::to_underlying

#if defined(_WIN32)
#    include <io.h> // _isatty, _fileno
#    define isatty _isatty
#    define fileno _fileno
#elif defined(__unix__)
#    include <unistd.h> // isatty, fileno
#endif

#ifndef NDEBUG
#    define logd(format, ...) logging::log(format, ##__VA_ARGS__)
#else
#    define logd(...) static_cast<void>(0)
#endif

namespace logging
{

inline bool have_color_stdout = false,
            have_color_stderr = false;

inline void check_color_support() noexcept
{
#if defined(_WIN32) || defined(__unix__)
    have_color_stdout = isatty(fileno(stdout)) != 0;
    have_color_stderr = isatty(fileno(stderr)) != 0;
#endif
}

enum class OutputType : bool
{
    StandardOutput,
    StandardError
};

enum class Color : std::uint8_t
{
    Default = 39,
    Black = 30,
    Red = 31,
    Green = 32,
    Yellow = 33,
    Blue = 34,
    Magenta = 35,
    Cyan = 36,
    White = 37,
    BrightBlack = 90,
    BrightRed = 91,
    BrightGreen = 92,
    BrightYellow = 93,
    BrightBlue = 94,
    BrightMagenta = 95,
    BrightCyan = 96,
    BrightWhite = 97
};

enum class Style : std::uint8_t
{
    Reset = 0,
    Bold = 1,
    Underline = 4
};

template<OutputType output_type = OutputType::StandardOutput>
constexpr std::string code(Color color) noexcept
{
    if constexpr (output_type == OutputType::StandardOutput)
    {
        if (!have_color_stdout)
            return {};
    }
    else if (!have_color_stderr)
        return {};

    return std::format("\u001b[{:d}m", std::to_underlying(color));
}

template<OutputType output_type = OutputType::StandardOutput>
constexpr std::string code(Style style) noexcept
{
    if constexpr (output_type == OutputType::StandardOutput)
    {
        if (!have_color_stdout)
            return {};
    }
    else if (!have_color_stderr)
        return {};

    return std::format("\u001b[{:d}m", std::to_underlying(style));
}

template<typename... Args>
struct log
{
    log(std::string_view fmt, Args&&... args, const std::source_location& loc = std::source_location::current())
    {
        std::print(stderr, "[{}{}{}] [{}{}DEBUG{}] {}:{}: {}\n", code<OutputType::StandardError>(Style::Bold), std::chrono::system_clock::now(), code<OutputType::StandardError>(Style::Reset), code<OutputType::StandardError>(Color::Blue), code<OutputType::StandardError>(Style::Bold), code<OutputType::StandardError>(Style::Reset), loc.file_name(), loc.line(), std::vformat(fmt, std::make_format_args(args...)));
    }

private:
};

template<typename... Args>
log(std::string_view, Args&&... args) -> log<Args...>;

} // namespace logging

#endif