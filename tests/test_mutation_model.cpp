#define BOOST_TEST_MODULE test_mutation_model
#include <boost/test/unit_test.hpp> // BOOST_AUTO_TEST_CASE

#include <array>      // std::array
#include <chrono>     // std::chrono::hours
#include <filesystem> // std::filesystem::exists, std::filesystem::last_write_time, std::filesystem::path
#include <format>     // std::format
#include <fstream>    // std::ofstream

#include <mutation.hpp> // MutationModel

BOOST_AUTO_TEST_CASE(empty_model)
{
    MutationModel mutation_model { "data/empty.mzn" };

    BOOST_REQUIRE_THROW(mutation_model.find_mutants({}), MutationModel::EmptyFile);

    BOOST_CHECK(!std::filesystem::exists("data/empty-mutants/"));
}

BOOST_AUTO_TEST_CASE(empty_mutant)
{
    constexpr auto model { "relational" };

    const auto model_filename { std::format("{:s}.mzn", model) };
    const std::filesystem::path mutant_folder_path { "data/empty-mutant-test" };
    const std::filesystem::path model_path { std::format("data/{:s}", model_filename) };

    BOOST_CHECK(!std::filesystem::exists(mutant_folder_path));

    MutationModel mutation_model { model_path, mutant_folder_path };

    BOOST_CHECK_NO_THROW(mutation_model.find_mutants());

    BOOST_CHECK(std::filesystem::exists(mutant_folder_path));

    // Select the first mutant that is not the normalized model and empty it.
    for (const auto& entry : std::filesystem::directory_iterator { mutant_folder_path })
    {
        if (entry.path() == model_path)
            std::cout << std::format("Ignoring {}", entry.path().string());

        std::filesystem::resize_file(entry.path(), 0);

        break;
    }

    // Here we only care about throwing the EmptyMutant exception, so just pass default values for the compiler path, arguments, etc.
    BOOST_CHECK_THROW(const auto result = mutation_model.run_mutants({}, {}, {}, {}, {}, {}, false, true), MutationModel::EmptyFile);

    mutation_model.clear_output_folder();

    BOOST_CHECK(!std::filesystem::exists(mutant_folder_path));
}

BOOST_AUTO_TEST_CASE(invalid_file)
{
    constexpr auto model { "relational" };

    const auto model_filename { std::format("{:s}.mzn", model) };
    const std::filesystem::path mutant_folder_path { "data/invalid-file-test" };
    const std::filesystem::path model_path { std::format("data/{:s}", model_filename) };

    BOOST_CHECK(!std::filesystem::exists(mutant_folder_path));

    MutationModel mutation_model { model_path, mutant_folder_path };

    BOOST_CHECK_NO_THROW(mutation_model.find_mutants());

    BOOST_CHECK(std::filesystem::exists(mutant_folder_path));

    // Create a file with a fake name.
    std::ofstream file { mutant_folder_path / "fake_file" };
    file << "% This is a fake file";

    // Here we only care about throwing the EmptyMutant exception, so just pass default values for the compiler path, arguments, etc.
    BOOST_CHECK_THROW(const auto result = mutation_model.run_mutants({}, {}, {}, {}, {}, {}, false, true), MutationModel::InvalidFile);

    BOOST_CHECK_THROW(mutation_model.clear_output_folder(), MutationModel::InvalidFile);

    BOOST_CHECK(std::filesystem::remove_all(mutant_folder_path));
}

BOOST_AUTO_TEST_CASE(no_mutants_detected)
{
    MutationModel mutation_model { "data/no_mutants.mzn" };

    BOOST_CHECK(!mutation_model.find_mutants({}));
}

BOOST_AUTO_TEST_CASE(outdated_mutant)
{
    constexpr auto model { "relational" };

    const auto model_filename { std::format("{:s}.mzn", model) };
    const std::filesystem::path mutant_folder_path { "data/outdated-mutant-test" };
    const std::filesystem::path model_path { std::format("data/{:s}", model_filename) };
    const std::filesystem::path normalized_model_path { mutant_folder_path / model_filename };

    BOOST_CHECK(!std::filesystem::exists(mutant_folder_path));

    MutationModel mutation_model { model_path, mutant_folder_path };

    BOOST_CHECK_NO_THROW(mutation_model.find_mutants());

    BOOST_CHECK(std::filesystem::exists(mutant_folder_path));

    // Set the time of the normalized model to the least possible (plus an hour) to make the original mutant newer,
    // thus triggering the outdated mutant exception.
    std::filesystem::last_write_time(normalized_model_path, std::filesystem::file_time_type::min() + std::chrono::hours { 1 });
    BOOST_REQUIRE(std::filesystem::last_write_time(model_path) > std::filesystem::last_write_time(normalized_model_path));

    // Here we only care about throwing the OutdatedMutant exception, so just pass default values for the compiler path, arguments, etc.
    BOOST_CHECK_THROW(const auto result = mutation_model.run_mutants({}, {}, {}, {}, {}, {}, false, true), MutationModel::OutdatedMutant);

    mutation_model.clear_output_folder();

    BOOST_CHECK(!std::filesystem::exists(mutant_folder_path));
}

BOOST_AUTO_TEST_CASE(unknown_operator)
{
    BOOST_REQUIRE_THROW(
        const MutationModel mutation_model("data/no_mutants.mzn", std::array { ascii_ci_string_view { "operator_that_does_not_exist" } }),
        MutationModel::UnknownOperator);
}