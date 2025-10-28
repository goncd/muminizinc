#define BOOST_TEST_MODULE test_execution
#include <boost/test/unit_test.hpp>

#include <array>       // std::array
#include <filesystem>  // std::filesystem::path
#include <format>      // std::format
#include <ranges>      // std::views::enumerate
#include <span>        // std::span
#include <string>      // std::string
#include <type_traits> // std::underlying_type_t
#include <utility>     // std::to_underlying

#include <boost/process/v2/environment.hpp> // boost::process::environment::find_executable

#include <case_insensitive_string.hpp> // ascii_ci_string_view

#include <mutation.hpp> // MuMiniZinc::clear_mutant_output_folder, MuMiniZinc::find_mutants, MuMiniZinc::find_mutants_args, MuMiniZinc::run_mutants, MuMiniZinc::run_mutants_args

using namespace std::string_literals;

namespace
{

const std::filesystem::path data_path { "data" };

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

BOOST_AUTO_TEST_CASE(aor)
{
    constexpr std::array operator_to_test { ascii_ci_string_view { "AOR" } };
    const auto path { data_path / "aor.mzn" };

    const std::array aor_data_files {
        "data/aor-1.dzn"s,
        "data/aor-2.dzn"s
    };

    constexpr std::array aor_results {
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

    perform_test(path, operator_to_test, aor_data_files, aor_results);
    perform_test(path, operator_to_test, aor_data_files, aor_results, data_path / "aor-execution");
}

BOOST_AUTO_TEST_CASE(cor)
{
    constexpr std::array operator_to_test { ascii_ci_string_view { "COR" } };
    const auto path { data_path / "cor.mzn" };

    constexpr std::array cor_results {
        0_status,
        0_status,
        0_status,
        0_status,
        1_status
    };

    perform_test(path, operator_to_test, {}, cor_results);
    perform_test(path, operator_to_test, {}, cor_results, data_path / "cor-execution");
}

BOOST_AUTO_TEST_CASE(fas)
{
    constexpr std::array operator_to_test { ascii_ci_string_view { "FAS" } };
    const auto path { data_path / "fas.mzn" };

    constexpr std::array fas_results {
        2_status,
        2_status,
        2_status,
        2_status,
        2_status,
    };

    perform_test(path, operator_to_test, {}, fas_results);
    perform_test(path, operator_to_test, {}, fas_results, data_path / "fas-execution");
}

BOOST_AUTO_TEST_CASE(fcr)
{
    constexpr std::array operator_to_test { ascii_ci_string_view { "FCR" } };
    const auto path { data_path / "fcr.mzn" };

    constexpr std::array fcr_results {
        0_status,
        1_status,
    };

    perform_test(path, operator_to_test, {}, fcr_results);
    perform_test(path, operator_to_test, {}, fcr_results, data_path / "fcr-execution");
}

BOOST_AUTO_TEST_CASE(ror)
{
    constexpr std::array operator_to_test { ascii_ci_string_view { "ROR" } };
    const auto path { data_path / "ror.mzn" };

    const std::array ror_data_files {
        "data/ror-1.dzn"s,
        "data/ror-2.dzn"s
    };

    constexpr std::array ror_results {
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

    perform_test(path, operator_to_test, ror_data_files, ror_results);
    perform_test(path, operator_to_test, ror_data_files, ror_results, data_path / "ror-execution");
}

BOOST_AUTO_TEST_CASE(sor)
{
    constexpr std::array operator_to_test { ascii_ci_string_view { "SOR" } };
    const auto path { data_path / "sor.mzn" };

    constexpr std::array sor_results {
        1_status,
        1_status,
        0_status,
        1_status
    };

    perform_test(path, operator_to_test, {}, sor_results);
    perform_test(path, operator_to_test, {}, sor_results, data_path / "sor-execution");
}

BOOST_AUTO_TEST_CASE(uod)
{
    constexpr std::array operator_to_test { ascii_ci_string_view { "UOD" } };
    const auto path { data_path / "uod.mzn" };

    const std::array uod_data_files {
        "data/uod-1.dzn"s,
    };

    constexpr std::array uod_results {
        0_status,
        0_status,
        1_status,
    };

    perform_test(path, operator_to_test, uod_data_files, uod_results);
    perform_test(path, operator_to_test, uod_data_files, uod_results, data_path / "uod-execution");
}