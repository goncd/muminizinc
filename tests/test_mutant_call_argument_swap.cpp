#define BOOST_TEST_MODULE test_mutant_call_argument_swap

#include <boost/test/unit_test.hpp>

#include <algorithm>   // std::ranges::any_of
#include <array>       // std::array
#include <string_view> // std::string_view

#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <mutation.hpp>                // MutationModel

BOOST_AUTO_TEST_CASE(call_argument_swap)
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

    MutationModel mutation_model { "data/call_argument_swap.mzn", std::array { ascii_ci_string_view { "SWP" } } };

    BOOST_REQUIRE_NO_THROW(BOOST_REQUIRE(mutation_model.find_mutants()));

    // Ignore the original mutant.
    const auto entries = mutation_model.get_memory().subspan(1);

    BOOST_REQUIRE(entries.size() == expected_mutants.size());

    for (auto mutant : expected_mutants)
        BOOST_REQUIRE(std::ranges::any_of(entries, [mutant](const auto& entry)
            { return entry.contents == mutant; }));
}