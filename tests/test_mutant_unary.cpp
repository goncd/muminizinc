#define BOOST_TEST_MODULE test_mutant_unary
#include <boost/test/unit_test.hpp>

#include <algorithm>   // std::ranges::any_of
#include <array>       // std::array
#include <format>      // std::format
#include <ranges>      // std::views::enumerate
#include <string_view> // std::string_view

#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <mutation.hpp>                // MuMiniZinc::find_mutants, MuMiniZinc::find_mutants_args

BOOST_AUTO_TEST_CASE(unary)
{
    using namespace std::string_view_literals;

    constexpr std::array expected_mutants {
        R"(int: value;
constraint value < 150;
constraint 150 > -value;
output  ["negative value is "++format(-value)++"\n"];
)"sv,
        R"(int: value;
constraint -value < 150;
constraint 150 > value;
output  ["negative value is "++format(-value)++"\n"];
)"sv,
        R"(int: value;
constraint -value < 150;
constraint 150 > -value;
output  ["negative value is "++format(value)++"\n"];
)"sv
    };

    constexpr std::array allowed_operators { ascii_ci_string_view { "UNA" } };

    const std::filesystem::path model_path { "data/unary.mzn" };

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