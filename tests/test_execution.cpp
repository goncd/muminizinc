#define BOOST_TEST_MODULE test_execution
#include <boost/test/unit_test.hpp>

#include <array>       // std::array
#include <filesystem>  // std::filesystem::path
#include <format>      // std::format
#include <ranges>      // std::views::enumerate
#include <span>        // std::span
#include <string>      // std::string
#include <string_view> // std::string_view
#include <type_traits> // std::underlying_type_t
#include <utility>     // std::to_underlying

#include <boost/process/v2/environment.hpp> // boost::process::environment::find_executable

#include <case_insensitive_string.hpp> // ascii_ci_string_view

#include <mutation.hpp> // MuMiniZinc::clear_mutant_output_folder, MuMiniZinc::find_mutants, MuMiniZinc::find_mutants_args, MuMiniZinc::run_mutants, MuMiniZinc::run_mutants_args

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace
{

constexpr auto operator""_status(unsigned long long value) noexcept
{
    return static_cast<std::underlying_type_t<MuMiniZinc::Entry::Status>>(value);
}

void perform_test(const std::filesystem::path& path, std::span<const ascii_ci_string_view> allowed_operators, std::span<const std::string> data_files, std::span<const std::uint8_t> results, const std::filesystem::path& output_directory = {})
{
    const MuMiniZinc::find_mutants_args find_parameters {
        .model = path,
        .allowed_operators = allowed_operators,
        .include_path = {},
        .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
    };

    auto entries = MuMiniZinc::find_mutants(find_parameters);

    BOOST_REQUIRE(!entries.mutants().empty());

    if (!output_directory.empty())
        MuMiniZinc::dump_mutants(entries, output_directory);

    const auto compiler_path = boost::process::environment::find_executable("minizinc");
    BOOST_REQUIRE(!compiler_path.empty());

    const MuMiniZinc::run_mutants_args run_parameters {
        .entry_result = entries,
        .compiler_path = compiler_path,
        .compiler_arguments = {},
        .allowed_mutants = {},
        .data_files = data_files,
        .timeout = std::chrono::seconds { 10 },
        .n_jobs = 0,
        .check_compiler_version = true,
        .output_log = {}
    };

    MuMiniZinc::run_mutants(run_parameters);

    if (!output_directory.empty())
        MuMiniZinc::clear_mutant_output_folder(path, output_directory);

    auto expected_result_iterator = results.begin();
    for (const auto& entry : entries.mutants())
    {
        for (const auto [index, value] : entry.results | std::views::enumerate)
        {
            // Check that we don't have more results than expected.
            BOOST_REQUIRE_MESSAGE(expected_result_iterator != results.end(), "There are more results than expected.");

            BOOST_CHECK_MESSAGE(*expected_result_iterator == std::to_underlying(value), std::format("{:s} (data file #{:d}){:s} Expected {:d}, got {:d}.", entry.name, index, output_directory.empty() ? " (in memory):" : ":", *expected_result_iterator, std::to_underlying(value)));

            ++expected_result_iterator;
        }
    }

    // Check that we don't have less results than expected.
    BOOST_REQUIRE_MESSAGE(expected_result_iterator == results.end(), "There are less results than expected.");
}

}

BOOST_AUTO_TEST_CASE(arithmetic)
{
    constexpr auto path { "data/arithmetic.mzn"sv };
    constexpr std::array operator_to_test { ascii_ci_string_view { "AOR" } };

    const std::array arithmetic_data_files {
        "data/arithmetic-1.dzn"s,
        "data/arithmetic-2.dzn"s
    };

    constexpr std::array arithmetic_results {
        0_status,
        1_status,
        1_status,
        0_status,
        0_status,
        1_status,
        0_status,
        1_status,
        0_status,
        1_status,
        1_status,
        2_status,
    };

    perform_test(path, operator_to_test, arithmetic_data_files, arithmetic_results);
    perform_test(path, operator_to_test, arithmetic_data_files, arithmetic_results, "data/arithmetic-execution");
}

BOOST_AUTO_TEST_CASE(boolean)
{
    constexpr auto path { "data/boolean.mzn"sv };
    constexpr std::array operator_to_test { ascii_ci_string_view { "COR" } };

    constexpr std::array boolean_results {
        0_status,
        0_status,
        0_status,
        0_status,
        1_status
    };

    perform_test(path, operator_to_test, {}, boolean_results);
    perform_test(path, operator_to_test, {}, boolean_results, "data/boolean-execution");
}

BOOST_AUTO_TEST_CASE(call_argument_swap)
{
    constexpr auto path { "data/call_argument_swap.mzn"sv };
    constexpr std::array operator_to_test { ascii_ci_string_view { "FAS" } };

    constexpr std::array call_argument_swap_results {
        2_status,
        2_status,
        2_status,
        2_status,
        2_status,
    };

    perform_test(path, operator_to_test, {}, call_argument_swap_results);
    perform_test(path, operator_to_test, {}, call_argument_swap_results, "data/call_argument_swap-execution");
}

BOOST_AUTO_TEST_CASE(call_swap)
{
    constexpr auto path { "data/call_swap.mzn"sv };
    constexpr std::array operator_to_test { ascii_ci_string_view { "FCR" } };

    constexpr std::array call_swap_results {
        0_status,
        1_status,
    };

    perform_test(path, operator_to_test, {}, call_swap_results);
    perform_test(path, operator_to_test, {}, call_swap_results, "data/call_swap-execution");
}

BOOST_AUTO_TEST_CASE(relational)
{
    constexpr auto path { "data/relational.mzn"sv };
    constexpr std::array operator_to_test { ascii_ci_string_view { "ROR" } };

    const std::array relational_data_files {
        "data/relational-1.dzn"s,
        "data/relational-2.dzn"s
    };

    constexpr std::array relational_results {
        0_status,
        0_status,
        1_status,
        1_status,
        1_status,
        1_status,
        1_status,
        0_status,
        0_status,
        1_status
    };

    perform_test(path, operator_to_test, relational_data_files, relational_results);
    perform_test(path, operator_to_test, relational_data_files, relational_results, "data/relational-execution");
}

BOOST_AUTO_TEST_CASE(set)
{
    constexpr auto path { "data/set.mzn"sv };
    constexpr std::array operator_to_test { ascii_ci_string_view { "SOR" } };

    constexpr std::array set_results {
        1_status,
        1_status,
        0_status,
        1_status
    };

    perform_test(path, operator_to_test, {}, set_results);
    perform_test(path, operator_to_test, {}, set_results, "data/set-execution");
}

BOOST_AUTO_TEST_CASE(unary)
{
    constexpr auto path { "data/unary.mzn"sv };
    constexpr std::array operator_to_test { ascii_ci_string_view { "UOD" } };

    const std::array unary_data_files {
        "data/unary-1.dzn"s,
    };

    constexpr std::array unary_results {
        0_status,
        0_status,
        1_status,
    };

    perform_test(path, operator_to_test, unary_data_files, unary_results);
    perform_test(path, operator_to_test, unary_data_files, unary_results, "data/unary-execution");
}