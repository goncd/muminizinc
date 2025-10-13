#define BOOST_TEST_MODULE test_mutant_call_argument_swap

#include <boost/test/unit_test.hpp>

#include <algorithm>   // std::ranges::any_of
#include <array>       // std::array
#include <filesystem>  // std::filesystem::path
#include <format>      // std::format
#include <ranges>      // std::views::enumerate
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

    constexpr std::array allowed_operators { ascii_ci_string_view { "SWP" } };

    const std::filesystem::path model_path { "data/call_argument_swap.mzn" };

    const MuMiniZinc::find_mutants_args find_parameters {
        .model = model_path,
        .allowed_operators = allowed_operators,
        .log_output = {},
        .include_path = {},
        .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
    };

    const auto entries = MuMiniZinc::find_mutants(find_parameters);

    const auto mutants = entries.mutants();
    BOOST_REQUIRE(mutants.size() == expected_mutants.size());

    for (auto [index, mutant] : expected_mutants | std::views::enumerate)
        BOOST_CHECK_MESSAGE(std::ranges::any_of(mutants, [mutant](const auto& entry)
                                { return entry.contents == mutant; }),
            std::format("Expected mutant #{:d} cannot be found among the result.", index));
}