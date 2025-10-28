#ifndef TEST_OPERATOR_UTILS
#define TEST_OPERATOR_UTILS

#include <boost/test/test_tools.hpp> // BOOST_CHECK_MESSAGE, BOOST_REQUIRE

#include <algorithm>   // std::ranges::any_of, std::ranges::find_if
#include <array>       // std::array
#include <cstddef>     // std::size_t
#include <filesystem>  // std::filesystem::path
#include <format>      // std::format
#include <iterator>    // std::distance
#include <ranges>      // std::views::enumerate
#include <span>        // std::span
#include <string_view> // std::string_view

#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <mutation.hpp>                // MuMiniZinc

template<const auto& allowed_operator>
void perform_test_operator(const std::filesystem::path& model_path, const std::span<const std::string_view> expected_mutants, std::size_t expected_occurrence)
{
    constexpr std::array allowed_operators { ascii_ci_string_view { allowed_operator } };

    const MuMiniZinc::find_mutants_args find_parameters {
        .model = model_path,
        .allowed_operators = allowed_operators,
        .include_path = {},
        .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
    };

    const auto entries = MuMiniZinc::find_mutants(find_parameters);
    const auto mutants = entries.mutants();

    constexpr auto it = std::ranges::find_if(MuMiniZinc::available_operators, [operator_name = allowed_operators.front()](const auto& element)
        { return element.first == operator_name; });
    static_assert(it != MuMiniZinc::available_operators.end(), "Couldn't find the requested operator");
    constexpr auto operator_id = static_cast<std::size_t>(std::distance(MuMiniZinc::available_operators.begin(), it));

    // Check that the correct amount of mutants have been generated and that they belong to the correct operator.
    BOOST_REQUIRE(expected_mutants.size() == mutants.size());
    BOOST_REQUIRE(expected_mutants.size() == entries.statistics()[operator_id].first);
    BOOST_REQUIRE(expected_occurrence == entries.statistics()[operator_id].second);

    // Check the contents of the generated mutants.
    for (auto [index, mutant] : expected_mutants | std::views::enumerate)
    {
        BOOST_CHECK_MESSAGE(std::ranges::any_of(mutants, [mutant](const auto& entry)
                                { return entry.contents == mutant; }),
            std::format("Expected mutant #{:d} cannot be found among the result.", index));
    }
}

template<const auto& allowed_operator>
void perform_test_operator(const std::filesystem::path& model_path, const std::span<const std::string_view> expected_mutants)
{
    perform_test_operator<allowed_operator>(model_path, expected_mutants, expected_mutants.size());
}

#endif