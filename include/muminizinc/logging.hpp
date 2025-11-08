#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <chrono>          // std::chrono::system_clock::now()
#include <cstdio>          // stdout, stderr
#include <filesystem>      // std::filesystem::path
#include <format>          // std::format, std::format_string
#include <memory>          // std::addressof
#include <ostream>         // std::print (std::ostream overload)
#include <print>           // std::print
#include <source_location> // std::source_location, std::source_location::current
#include <string_view>     // std::string_view
#include <type_traits>     // std::is_same_v
#include <utility>         // std::forward, std::to_underlying

#if defined(_WIN32)
#    include <io.h> // _isatty, _fileno
#    define isatty _isatty
#    define fileno _fileno
#elif defined(__unix__)
#    include <unistd.h> // isatty, fileno
#endif

/** Convenience macro for logging which does nothing if this is not a debug build. */
#ifndef NDEBUG
#    define logd(format, ...) logging::log(format, ##__VA_ARGS__)
#else
#    define logd(...) static_cast<void>(0)
#endif

/**
 * @file
 * @brief The logging library.
 */
namespace logging
{

/**
 * Returns a reference to the internal representation of the given path if it's already UTF-8,
 * with no overhead. If the path's internal representation not UTF-8, a conversion is made.
 *
 * @param path the path to convert to UTF-8.
 *
 * @return a reference to the internal representation of the path if it's already UTF-8
 *         or a copy of the path converted to UTF-8.
 */
inline decltype(auto) path_to_utf8(const std::filesystem::path& path)
{
    if constexpr (std::is_same_v<std::filesystem::path::string_type, std::string>)
        return path.native();
    else
    {
        const auto str { path.u8string() };

        return std::string(reinterpret_cast<const char*>(str.data()), str.size());
    }
}

/** The type of log outout. */
enum class OutputType : bool
{
    /** Standard Output (stdout) */
    StandardOutput,
    /** Standard Error (stderr) */
    StandardError
};

/** Class for handling support for colors of the terminal. */
class color_support
{
    static inline bool have_color_stdout = false,
                       have_color_stderr = false;

public:
    color_support() = delete;

    /** Determines if the terminal has color support. */
    static void check() noexcept
    {
#if defined(_WIN32) || defined(__unix__)
        have_color_stdout = isatty(fileno(stdout)) != 0;
        have_color_stderr = isatty(fileno(stderr)) != 0;
#endif
    }

    /** Manually sets the terminal color support. */
    static void set(bool color_stdout, bool color_stderr) noexcept
    {
        have_color_stdout = color_stdout;
        have_color_stderr = color_stderr;
    }

    /**
     * @return true or false whether the determined output type has color support.
     */
    template<OutputType output_type = OutputType::StandardOutput>
    [[nodiscard]] static constexpr bool get() noexcept
    {
        if constexpr (output_type == OutputType::StandardOutput)
        {
            if (!have_color_stdout)
                return false;
        }
        else if (!have_color_stderr)
            return false;

        return true;
    }
};

/** The colors that can be used to format the output. */
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

/** The styles that can be used to format the output. Style::Reset resets the format. */
enum class Style : std::uint8_t
{
    Reset = 0,
    Bold = 1,
    Underline = 4
};

/**
 * Determines the ANSI escape code corresponding to the provided color or style and returns it.
 * If there is no color supported on the specified output, an empty string will be returned.
 *
 * @param value the provided color or style.
 * @return a std::string with the corresponding ANSI escape code.
 */
template<OutputType output_type = OutputType::StandardOutput>
constexpr std::string code(auto&& value) noexcept
    requires(std::same_as<std::remove_cvref_t<decltype(value)>, Color> || std::same_as<std::remove_cvref_t<decltype(value)>, Style>)
{
    if (!color_support::get<output_type>())
        return {};

    return std::format("\u001b[{:d}m", std::to_underlying(value));
}

/**
 * Returns the carriage return string or nothing if the specified output does not support color.
 *
 * @return a `std::string_view` which points to a static allocated string of a carriage return character,
 *         or an empty view.
 */
template<OutputType output_type = OutputType::StandardOutput>
constexpr std::string_view carriage_return() noexcept
{
    if (!color_support::get<output_type>())
        return {};

    return "\r";
}

/** The logging struct. */
template<typename... Args>
struct log
{
    /** Logs to `stderr`, appending the date, time and line. */
    log(std::format_string<Args...> fmt, Args&&... args, const std::source_location& loc = std::source_location::current())
    {
        std::print(stderr, "[{}{}{}] [{}{}DEBUG{}] {}:{}: {}\n", code<OutputType::StandardError>(Style::Bold), std::chrono::system_clock::now(), code<OutputType::StandardError>(Style::Reset), code<OutputType::StandardError>(Color::Blue), code<OutputType::StandardError>(Style::Bold), code<OutputType::StandardError>(Style::Reset), loc.file_name(), loc.line(), std::format(fmt, std::forward<Args>(args)...));
    }
};

/** Deduction guides for log. */
template<typename... Args>
log(std::format_string<Args...> fmt, Args&&... args) -> log<Args...>;

/**
 * A wrapper that may hold or may not hold a reference to a `std::ostream`.
 */
class output
{
public:
    /** This wrapper holds a reference to this type */
    using T = std::ostream;

    /** Creates an output with a valid `std::ostream` inside. */
    constexpr explicit output(T& ostream) noexcept :
        m_ostream { std::addressof(ostream) } { };

    /** Creates an output that does not hold any `std::ostream`. */
    constexpr output() noexcept :
        m_ostream(nullptr) { }

    /**
     * Equivalent to the `std::ostream` overload of std::print.
     * Does nothing if there is no valid reference.
     */
    template<typename... Args>
    void print(std::format_string<Args...> fmt, Args&&... args)
    {
        if (m_ostream != nullptr)
            std::print(*m_ostream, fmt, std::forward<Args>(args)...);
    }

    /**
     * Equivalent to the `std::ostream` overload of std::println.
     * Does nothing if there is no valid reference.
     */
    template<typename... Args>
    void println(std::format_string<Args...> fmt, Args&&... args)
    {
        if (m_ostream != nullptr)
            std::println(*m_ostream, fmt, std::forward<Args>(args)...);
    }

    /**
     * Equivalent to the `std::ostream` overload of `std::print`.
     * Does nothing if there is no valid reference.
     */
    void println()
    {
        if (m_ostream != nullptr)
            std::print(*m_ostream, "\n");
    }

    /**
     * Unchecked const getter.
     *
     * @return a pointer to the reference that this object is holding, which may be `nullptr`.
     */
    [[nodiscard]] constexpr const T* get_stream() const noexcept { return m_ostream; }

    /**
     * Unchecked getter.
     *
     * @return a pointer to the reference that this object is holding, which may be `nullptr`.
     */
    [[nodiscard]] constexpr T* get_stream() noexcept { return m_ostream; }

    /**
     * Checks if this object holds a reference or not.
     *
     * @return true if it's holding a value or false if not.
     */
    [[nodiscard]] constexpr bool has_value() const noexcept { return m_ostream != nullptr; }

    /**
     * Implicit conversion operator, equivalent to has_value().
     *
     * @return true if it's holding a value or false if not.
     */
    [[nodiscard]] constexpr operator bool() const noexcept { return has_value(); };

private:
    T* m_ostream;
};

} // namespace logging

#endif