#ifndef CASE_INSENSITIVE_STRING
#define CASE_INSENSITIVE_STRING

#include <string>      // std::basic_string, std::char_traits
#include <string_view> // std::basic_string_view

/**
 * @file
 * @brief Case insensitive ASCII-only `std::string` and `std::string_view` alternatives.
 */

/** 
 * Case insensitive ASCII-only char traits. Non-ASCII characters
 * are invisible and not being used for comparisons.
 */
struct ascii_ci_char_traits : public std::char_traits<char>
{
private:
    // Fast ASCII-only `tolower`.
    static constexpr char to_lower_ascii(char c) noexcept
    {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }

public:

    /** ASCII-only case insensitive equal function. */
    static constexpr bool eq(char c1, char c2) noexcept
    {
        return to_lower_ascii(c1) == to_lower_ascii(c2);
    }

    /** ASCII-only case insensitive less than function. */
    static constexpr bool lt(char c1, char c2) noexcept
    {
        return to_lower_ascii(c1) < to_lower_ascii(c2);
    }

    /** ASCII-only case insensitive find comparison. */
    static constexpr int compare(const char* s1, const char* s2, std::size_t n) noexcept
    {
        for (std::size_t i {}; i < n; ++i)
        {
            const auto c1 = to_lower_ascii(s1[i]);
            const auto c2 = to_lower_ascii(s2[i]);

            if (c1 < c2)
                return -1;

            if (c1 > c2)
                return 1;
        }

        return 0;
    }

    /** ASCII-only case insensitive find function. */
    static constexpr const char* find(const char* s, std::size_t n, char a) noexcept
    {
        const auto lower_a { to_lower_ascii(a) };

        while (n-- != 0)
        {
            if (to_lower_ascii(*s) == lower_a)
                return s;
            ++s;
        }

        return nullptr;
    }
};

/** ascii_ci_string: ASCII-only case insensitive string */
using ascii_ci_string = std::basic_string<char, ascii_ci_char_traits>;

/** ascii_ci_string_view: ASCII-only case insensitive string_view */
using ascii_ci_string_view = std::basic_string_view<char, ascii_ci_char_traits>;

/** Converts a `std::string_view` to a ascii_ci_string_view. */
constexpr auto to_ascii_ci_string_view(std::string_view a) noexcept
{
    return ascii_ci_string_view { a.data(), a.size() };
}

/** Performs a case insensitive comparison between an ascii_ci_string_view and a `std::string_view`. */
constexpr bool operator==(ascii_ci_string_view lhs, std::string_view rhs) noexcept
{
    return (lhs <=> to_ascii_ci_string_view(rhs)) == 0;
}

/** Performs a case insensitive comparison between a `std::string_view` and an ascii_ci_string_view. */
constexpr bool operator==(std::string_view lhs, ascii_ci_string_view rhs) noexcept
{
    return rhs == lhs;
}

#endif