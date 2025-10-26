#define BOOST_TEST_MODULE test_mutation_model
#include <boost/test/unit_test.hpp>

#include <array>      // std::array
#include <chrono>     // std::chrono::hours
#include <filesystem> // std::filesystem::exists, std::filesystem::last_write_time, std::filesystem::path
#include <fstream>    // std::ofstream

#include <executor.hpp> // MuMiniZinc::UnknownMutant
#include <mutation.hpp> // MuMiniZinc::clear_mutant_output_folder, MuMiniZinc::find_mutants, MuMiniZinc::find_mutants_args, MuMiniZinc::retrieve_mutants, MuMiniZinc::retrieve_mutants_args, MuMiniZinc::run_mutants, MuMiniZinc::run_mutants_args

namespace
{
const std::filesystem::path data_path { "data" };
}

BOOST_AUTO_TEST_CASE(empty_model)
{
    const std::filesystem::path model_path { data_path / "empty.mzn" };

    const MuMiniZinc::find_mutants_args find_parameters {
        .model = model_path,
        .allowed_operators = {},
        .include_path = {},
        .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
    };

    BOOST_REQUIRE_THROW(const auto entries = MuMiniZinc::find_mutants(find_parameters), MuMiniZinc::EmptyFile);
}

BOOST_AUTO_TEST_CASE(empty_mutant)
{
    constexpr auto model_filename { "relational.mzn" };
    const auto model_path { data_path / model_filename };
    const auto mutant_folder_path { data_path / "empty-mutant-test" };
    const auto normalized_model_path { mutant_folder_path / model_filename };

    BOOST_REQUIRE(!std::filesystem::exists(mutant_folder_path));

    const MuMiniZinc::find_mutants_args find_parameters {
        .model = model_path,
        .allowed_operators = {},
        .include_path = {},
        .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
    };

    const auto entries = MuMiniZinc::find_mutants(find_parameters);

    MuMiniZinc::dump_mutants(entries, mutant_folder_path);

    BOOST_REQUIRE(std::filesystem::exists(mutant_folder_path));

    // Select the first mutant that is not the normalized model and empty it.
    for (const auto& entry : std::filesystem::directory_iterator { mutant_folder_path })
    {
        if (entry.path() == normalized_model_path)
            continue;

        std::filesystem::resize_file(entry.path(), 0);

        break;
    }

    const MuMiniZinc::retrieve_mutants_args retrieve_parameters {
        .model_path = model_path,
        .directory_path = mutant_folder_path,
        .allowed_operators = {},
        .allowed_mutants = {},
        .check_model_last_modified_time = true
    };

    BOOST_CHECK_THROW(const auto new_entries = MuMiniZinc::retrieve_mutants(retrieve_parameters), MuMiniZinc::EmptyFile);

    MuMiniZinc::clear_mutant_output_folder(model_path, mutant_folder_path);

    BOOST_REQUIRE(!std::filesystem::exists(mutant_folder_path));
}

BOOST_AUTO_TEST_CASE(invalid_file)
{
    constexpr auto model_filename { "relational.mzn" };
    const auto model_path { data_path / model_filename };
    const auto mutant_folder_path { data_path / "invalid-file-test" };

    BOOST_REQUIRE(!std::filesystem::exists(mutant_folder_path));

    const MuMiniZinc::find_mutants_args find_parameters {
        .model = model_path,
        .allowed_operators = {},
        .include_path = {},
        .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
    };

    const auto entries = MuMiniZinc::find_mutants(find_parameters);

    MuMiniZinc::dump_mutants(entries, mutant_folder_path);

    BOOST_REQUIRE(std::filesystem::exists(mutant_folder_path));

    const MuMiniZinc::retrieve_mutants_args retrieve_parameters {
        .model_path = model_path,
        .directory_path = mutant_folder_path,
        .allowed_operators = {},
        .allowed_mutants = {},
        .check_model_last_modified_time = true
    };

    // First check that we can retrieve the mutants perfectly fine before adding a file.
    BOOST_CHECK_NO_THROW(const auto new_entries = MuMiniZinc::retrieve_mutants(retrieve_parameters));

    // Create a file with a fake name.
    std::ofstream file { mutant_folder_path / "fake_file" };
    file << "% This is a fake file";

    BOOST_REQUIRE(file.good());

    // Manually close the file so we can remove the folder later.
    file.close();

    BOOST_CHECK_THROW(const auto new_entries = MuMiniZinc::retrieve_mutants(retrieve_parameters), MuMiniZinc::InvalidFile);

    BOOST_REQUIRE(std::filesystem::remove_all(mutant_folder_path));

    BOOST_REQUIRE(!std::filesystem::exists(mutant_folder_path));
}

BOOST_AUTO_TEST_CASE(no_mutants_detected)
{
    const auto model_path { data_path / "no_mutants.mzn" };

    const MuMiniZinc::find_mutants_args find_parameters {
        .model = model_path,
        .allowed_operators = {},
        .include_path = {},
        .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
    };

    const auto entries = MuMiniZinc::find_mutants(find_parameters);

    BOOST_REQUIRE(entries.mutants().empty());
}

BOOST_AUTO_TEST_CASE(outdated_mutant)
{
    constexpr auto model_filename { "relational.mzn" };
    const auto mutant_folder_path { data_path / "outdated-mutant-test" };
    const auto model_path { data_path / model_filename };
    const auto normalized_model_path { mutant_folder_path / model_filename };

    BOOST_REQUIRE(!std::filesystem::exists(mutant_folder_path));

    const MuMiniZinc::find_mutants_args find_parameters {
        .model = model_path,
        .allowed_operators = {},
        .include_path = {},
        .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
    };

    const auto entries = MuMiniZinc::find_mutants(find_parameters);

    MuMiniZinc::dump_mutants(entries, mutant_folder_path);

    BOOST_REQUIRE(std::filesystem::exists(mutant_folder_path));

    // Set the last write time of the normalised model to an hour before the original model to make it older,
    // thus triggering the outdated mutant exception.
    const auto model_last_write_time { std::filesystem::last_write_time(model_path) };
    std::filesystem::last_write_time(normalized_model_path, model_last_write_time - std::chrono::hours(1));

    BOOST_REQUIRE(model_last_write_time > std::filesystem::last_write_time(normalized_model_path));

    const MuMiniZinc::retrieve_mutants_args retrieve_parameters {
        .model_path = model_path,
        .directory_path = mutant_folder_path,
        .allowed_operators = {},
        .allowed_mutants = {},
        .check_model_last_modified_time = true
    };

    BOOST_CHECK_THROW(const auto new_entries = MuMiniZinc::retrieve_mutants(retrieve_parameters), MuMiniZinc::OutdatedMutant);

    MuMiniZinc::clear_mutant_output_folder(model_path, mutant_folder_path);

    BOOST_REQUIRE(!std::filesystem::exists(mutant_folder_path));
}

BOOST_AUTO_TEST_CASE(unknown_operator)
{
    constexpr std::array unknown_operators { ascii_ci_string_view { "operator_that_does_not_exist" } };
    const auto model_path { data_path / "no_mutants.mzn" };

    const MuMiniZinc::find_mutants_args find_parameters {
        .model = model_path,
        .allowed_operators = unknown_operators,
        .include_path = {},
        .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
    };

    BOOST_REQUIRE_THROW(const auto entries = MuMiniZinc::find_mutants(find_parameters), MuMiniZinc::UnknownOperator);
}

BOOST_AUTO_TEST_CASE(unknown_mutant)
{
    constexpr std::array unknown_mutant { ascii_ci_string_view { "this_mutant_does_not_exist" } };
    const auto model_path { data_path / "relational.mzn" };

    const MuMiniZinc::find_mutants_args find_parameters {
        .model = model_path,
        .allowed_operators = {},
        .include_path = {},
        .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
    };

    auto entries = MuMiniZinc::find_mutants(find_parameters);

    const std::filesystem::path fake_compiler { "fake_compiler" };

    const MuMiniZinc::run_mutants_args run_parameters {
        .entry_result = entries,
        .compiler_path = fake_compiler,
        .compiler_arguments = {},
        .allowed_mutants = unknown_mutant,
        .data_files = {},
        .timeout = std::chrono::seconds { 10 },
        .n_jobs = 0,
        .check_compiler_version = true,
        .output_log = {}
    };

    BOOST_REQUIRE_THROW(MuMiniZinc::run_mutants(run_parameters), MuMiniZinc::UnknownMutant);
}