#define BOOST_TEST_MODULE test_mutant_set
#include <boost/test/unit_test.hpp>

#include <algorithm>   // std::ranges::any_of
#include <array>       // std::array
#include <filesystem>  // std::filesystem::path
#include <format>      // std::format
#include <ranges>      // std::views::enumerate
#include <string_view> // std::string_view

#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <mutation.hpp>                // MuMiniZinc::find_mutants, MuMiniZinc::find_mutants_args

BOOST_AUTO_TEST_CASE(set)
{
    using namespace std::string_view_literals;

    constexpr std::array expected_mutants {
        R"(set of int: a = {1, 3, 6, 7, 8};
set of int: b = {3, 7, 8};
set of int: c = {1, 6};
constraint a subset b;
constraint b union c==a;
)"sv,
        R"(set of int: a = {1, 3, 6, 7, 8};
set of int: b = {3, 7, 8};
set of int: c = {1, 6};
constraint a superset b;
constraint b diff c==a;
)"sv,
        R"(set of int: a = {1, 3, 6, 7, 8};
set of int: b = {3, 7, 8};
set of int: c = {1, 6};
constraint a superset b;
constraint b symdiff c==a;
)"sv,
        R"(set of int: a = {1, 3, 6, 7, 8};
set of int: b = {3, 7, 8};
set of int: c = {1, 6};
constraint a superset b;
constraint b intersect c==a;
)"sv
    };

    constexpr std::array allowed_operators { ascii_ci_string_view { "SET" } };
    const std::filesystem::path model_path { "data/set.mzn" };

    const MuMiniZinc::find_mutants_args find_parameters {
        .model = model_path,
        .allowed_operators = allowed_operators,
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