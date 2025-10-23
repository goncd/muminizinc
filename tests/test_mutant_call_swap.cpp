#define BOOST_TEST_MODULE test_mutant_call_swap
#include <boost/test/unit_test.hpp>

#include <algorithm>   // std::ranges::any_of
#include <array>       // std::array
#include <filesystem>  // std::filesystem::path
#include <format>      // std::format
#include <ranges>      // std::views::enumerate
#include <string_view> // std::string_view

#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <mutation.hpp>                // MuMiniZinc::find_mutants, MuMiniZinc::find_mutants_args

BOOST_AUTO_TEST_CASE(call_swap)
{
    using namespace std::string_view_literals;

    constexpr std::array expected_mutants {
        R"(set of int: arr1 = 0..5;
set of int: arr2 = 0..5;
constraint exists ( x in arr1 ) ( x>=0 );
constraint exists ( x in arr2 ) ( x > 3 );
)"sv,
        R"(set of int: arr1 = 0..5;
set of int: arr2 = 0..5;
constraint forall ( x in arr1 ) ( x>=0 );
constraint forall ( x in arr2 ) ( x > 3 );
)"sv
    };

    constexpr std::array allowed_operators { ascii_ci_string_view { "FCR" } };

    const std::filesystem::path model_path { "data/call_swap.mzn" };

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