#define BOOST_TEST_MODULE test_execution
#include <boost/test/unit_test.hpp>

#include <boost/process/v2/environment.hpp> // boost::process::environment::find_executable

#include <mutation.hpp> // MutationModel
#include <ranges>       // std::views::drop
#include <string>       // std::string
#include <string_view>  // std::string_view
#include <type_traits>  // std::underlying_type

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace
{

constexpr auto operator""_status(unsigned long long value) noexcept
{
    return static_cast<std::underlying_type_t<MutationModel::Entry::Status>>(value);
}

void perform_test(const std::filesystem::path& path, std::span<const ascii_ci_string_view> allowed_operators, std::span<const std::string> data_files, std::span<const std::uint8_t> results, const std::filesystem::path& output_directory = {})
{
    MutationModel mutation_model;

    if (output_directory.empty())
        mutation_model = MutationModel { path, allowed_operators };
    else
        mutation_model = MutationModel { path, output_directory, allowed_operators };

    BOOST_REQUIRE(mutation_model.find_mutants());
    const auto executable = boost::process::environment::find_executable("minizinc");

    BOOST_REQUIRE(!executable.empty());

    mutation_model.run_mutants(executable, {}, data_files, std::chrono::seconds { 10 }, 0, {}, true, true);

    if (!output_directory.empty())
        mutation_model.clear_output_folder();

    auto expected_result_iterator = results.begin();
    for (const auto& entry : mutation_model.get_entries() | std::views::drop(1))
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

    perform_test(path, std::array { ascii_ci_string_view { "ART" } }, arithmetic_data_files, arithmetic_results);
    perform_test(path, std::array { ascii_ci_string_view { "ART" } }, arithmetic_data_files, arithmetic_results, "data/arithmetic-execution");
}

BOOST_AUTO_TEST_CASE(call_argument_swap)
{
    constexpr auto path { "data/call_argument_swap.mzn"sv };

    constexpr std::array call_argument_swap_results {
        2_status,
        2_status,
        2_status,
        2_status,
        2_status,
    };

    perform_test(path, std::array { ascii_ci_string_view { "SWP" } }, {}, call_argument_swap_results);
    perform_test(path, std::array { ascii_ci_string_view { "SWP" } }, {}, call_argument_swap_results, "data/call_argument_swap-execution");
}

BOOST_AUTO_TEST_CASE(call_swap)
{
    constexpr auto path { "data/call_swap.mzn"sv };

    constexpr std::array call_swap_results {
        0_status,
        1_status,
    };

    perform_test(path, std::array { ascii_ci_string_view { "CALL" } }, {}, call_swap_results);
    perform_test(path, std::array { ascii_ci_string_view { "CALL" } }, {}, call_swap_results, "data/call_swap-execution");
}

BOOST_AUTO_TEST_CASE(relational)
{
    constexpr auto path { "data/relational.mzn"sv };

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

    perform_test(path, std::array { ascii_ci_string_view { "REL" } }, relational_data_files, relational_results);
    perform_test(path, std::array { ascii_ci_string_view { "REL" } }, relational_data_files, relational_results, "data/relational-execution");
}

BOOST_AUTO_TEST_CASE(unary)
{
    constexpr auto path { "data/unary.mzn"sv };

    const std::array unary_data_files {
        "data/unary-1.dzn"s,
    };

    constexpr std::array unary_results {
        0_status,
        1_status,
    };

    perform_test(path, std::array { ascii_ci_string_view { "UNA" } }, unary_data_files, unary_results);
    perform_test(path, std::array { ascii_ci_string_view { "UNA" } }, unary_data_files, unary_results, "data/unary-execution");
}