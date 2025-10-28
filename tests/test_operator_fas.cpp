#define BOOST_TEST_MODULE test_operator_fas
#include <boost/test/unit_test.hpp>

#include <array>       // std::array
#include <string_view> // std::string_view

#include "test_operator_utils.hpp" // perform_test_operator

namespace
{
inline constexpr std::string_view allowed_operator { "FAS" };
}

BOOST_AUTO_TEST_CASE(fas)
{
    using namespace std::string_view_literals;

    constexpr std::array expected_mutants {
        R"(string: str = "hello";
float: flo = 5.3214;
output  [((((("\""++str)++"\" has ")++format(string_length(str)))++
        " characters\nflo's value is ")++
        show_float(4, -5, flo))++
        "\n"];
)"sv,
        R"(string: str = "hello";
float: flo = 5.3214;
output  [((((("\""++str)++"\" has ")++format(string_length(str)))++
        " characters\nflo's value is ")++
        show_float(4, flo, -5))++
        "\n"];
)"sv,
        R"(string: str = "hello";
float: flo = 5.3214;
output  [((((("\""++str)++"\" has ")++format(string_length(str)))++
        " characters\nflo's value is ")++
        show_float(-5, flo, 4))++
        "\n"];
)"sv,
        R"(string: str = "hello";
float: flo = 5.3214;
output  [((((("\""++str)++"\" has ")++format(string_length(str)))++
        " characters\nflo's value is ")++
        show_float(flo, 4, -5))++
        "\n"];
)"sv,
        R"(string: str = "hello";
float: flo = 5.3214;
output  [((((("\""++str)++"\" has ")++format(string_length(str)))++
        " characters\nflo's value is ")++
        show_float(flo, -5, 4))++
        "\n"];
)"sv
    };

    perform_test_operator<allowed_operator>("data/fas.mzn", expected_mutants);
}