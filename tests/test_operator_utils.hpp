#ifndef TEST_OPERATOR_UTILS
#define TEST_OPERATOR_UTILS

#include <boost/test/test_tools.hpp> // BOOST_CHECK_MESSAGE, BOOST_REQUIRE

#include <algorithm>   // std::ranges::any_of, std::ranges::equal, std::ranges::find_if
#include <array>       // std::array
#include <cstddef>     // std::size_t
#include <filesystem>  // std::filesystem::path
#include <format>      // std::format
#include <iterator>    // std::distance
#include <ranges>      // std::views::enumerate
#include <span>        // std::span
#include <string_view> // std::string_view
#include <utility>     // std::to_underlying

#include <boost/process/v2/environment.hpp> // boost::process::environment::find_executable

#include <muminizinc/case_insensitive_string.hpp> // ascii_ci_string_view
#include <muminizinc/mutation.hpp>                // MuMiniZinc::clear_mutant_output_folder, MuMiniZinc::Entry::Status, MuMiniZinc::find_mutants, MuMiniZinc::find_mutants_args, MuMiniZinc::run_mutants, MuMiniZinc::run_mutants_args
#include <muminizinc/operators.hpp>               // MuMiniZinc::available_operators

using Status = MuMiniZinc::Entry::Status;

inline const std::filesystem::path data_path { "data" };

inline void perform_test_execution(const std::filesystem::path& path, std::span<const ascii_ci_string_view> allowed_operators, std::span<const std::string> data_files, std::span<const Status> results, const std::filesystem::path& output_directory)
{
    const MuMiniZinc::find_mutants_args find_parameters {
        .model = path,
        .allowed_operators = allowed_operators,
        .include_path = {},
        .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
    };

    auto entries = MuMiniZinc::find_mutants(find_parameters);

    BOOST_REQUIRE(!entries.mutants().empty());

    MuMiniZinc::dump_mutants(entries, output_directory);

    const MuMiniZinc::retrieve_mutants_args retrieve_parameters {
        .model_path = path,
        .directory_path = output_directory,
        .allowed_operators = allowed_operators,
        .allowed_mutants = {},
        .check_model_last_modified_time = true
    };

    const auto dumped_entries = MuMiniZinc::retrieve_mutants(retrieve_parameters);

    MuMiniZinc::clear_mutant_output_folder(path, output_directory);

    // Check that the generated mutants and the dumped mutants are the exact same.
    BOOST_REQUIRE(std::ranges::equal(entries.mutants(), dumped_entries.mutants()));
    BOOST_REQUIRE(entries.model_name() == dumped_entries.model_name());
    BOOST_REQUIRE(entries.normalized_model() == dumped_entries.normalized_model());

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

    auto expected_result_iterator = results.begin();
    for (const auto& entry : entries.mutants())
    {
        for (const auto [index, value] : entry.results | std::views::enumerate)
        {
            // Check that we don't have more results than expected.
            BOOST_REQUIRE_MESSAGE(expected_result_iterator != results.end(), "There are more results than expected.");

            BOOST_CHECK_MESSAGE(*expected_result_iterator == value, std::format("{:s} (data file #{:d}){:s} Expected {:d}, got {:d}.", entry.name, index, output_directory.empty() ? " (in memory):" : ":", std::to_underlying(*expected_result_iterator), std::to_underlying(value)));

            ++expected_result_iterator;
        }
    }

    // Check that we don't have less results than expected.
    BOOST_REQUIRE_MESSAGE(expected_result_iterator == results.end(), "There are less results than expected.");
}

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
    static_assert(MuMiniZinc::available_operators.size() == entries.statistics().size(), "The size of the statistics must be equal to the number of available operators.");
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