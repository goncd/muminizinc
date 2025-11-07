#define BOOST_TEST_MODULE test_operator_fas
#include <boost/test/unit_test.hpp>

#include <array>       // std::array
#include <string_view> // std::string_view

#include "test_operator_utils.hpp" // perform_test_execution, perform_test_operator, Status

namespace
{
inline constexpr std::array operator_to_test { ascii_ci_string_view { "FAS" } };
inline const auto path { data_path / "fas.mzn" };
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

    perform_test_operator<operator_to_test.front()>(path, expected_mutants);
}

BOOST_AUTO_TEST_CASE(fas_execution)
{
    constexpr std::array results {
        Status::Invalid,
        Status::Invalid,
        Status::Invalid,
        Status::Invalid,
        Status::Invalid,
    };

    perform_test_execution(path, operator_to_test, {}, results, data_path / "fas-execution");
}