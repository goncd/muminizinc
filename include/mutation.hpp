#ifndef MUTATION_HPP
#define MUTATION_HPP

#include <array>       // std::array
#include <filesystem>  // std::filesystem::path
#include <functional>  // std::reference_wrapper
#include <span>        // std::span
#include <string>      // std::string
#include <string_view> // std::string_view
#include <utility>     // std::move
#include <variant>     // std::variant
#include <vector>      // std::vector

#include <minizinc/config.hh> // MZN_VERSION_MAJOR, MZN_VERSION_MINOR, MZN_VERSION_PATCH
#include <minizinc/model.hh>  // MiniZinc::Model

#define MZN_VERSION MZN_VERSION_MAJOR "." MZN_VERSION_MINOR "." MZN_VERSION_PATCH
#define MZN_VERSION_FULL "version " MZN_VERSION

#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <logging.hpp>                 // logging::output
#include <operators.hpp>               // MuMiniZinc::available_operators

namespace MuMiniZinc
{

class OutdatedMutant : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class IOError : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class EmptyFile : public IOError
{
    using IOError::IOError;
};

class InvalidFile : public IOError
{
    using IOError::IOError;
};

class EntryResult;
struct find_mutants_args;
struct retrieve_mutants_args;
struct run_mutants_args;

struct Entry
{
    constexpr Entry(std::string name, std::string contents) noexcept :
        name { std::move(name) }, contents { std::move(contents) } { }

    constexpr Entry() noexcept = default;

    enum class Status : std::uint8_t
    {
        Alive,
        Dead,
        Invalid
    };

    // The result of the mutant tested against every data file.
    std::vector<Status> results;

    std::string name;
    std::string contents;
};

class EntryResult
{
    std::vector<Entry> m_mutants;
    std::array<std::pair<std::uint64_t, std::uint64_t>, available_operators.size()> m_statistics;

    std::string m_model_name;
    std::string m_model_contents;

    friend EntryResult find_mutants(const find_mutants_args& parameters);
    friend EntryResult retrieve_mutants(const retrieve_mutants_args& parameters);
    friend void run_mutants(const run_mutants_args& parameters);
    friend class Mutator;

    void save_model(const MiniZinc::Model* model, std::string_view operator_name, std::uint64_t location_id, std::uint64_t occurrence_id, std::span<const std::pair<std::string, std::string>> detected_enums);

public:
    [[nodiscard]] constexpr std::span<const Entry> mutants() const noexcept { return m_mutants; }
    [[nodiscard]] constexpr std::string_view model_name() const noexcept { return m_model_name; }
    [[nodiscard]] constexpr std::string_view normalized_model() const noexcept { return m_model_contents; }
    [[nodiscard]] constexpr std::span<const std::pair<std::uint64_t, std::uint64_t>> statistics() const noexcept { return m_statistics; }
};

struct find_mutants_args
{
    struct ModelDetails
    {
        std::string name;
        std::string contents;
    };

    // The model source can be either a string or a file.
    std::variant<ModelDetails, std::reference_wrapper<const std::filesystem::path>> model;

    std::span<const ascii_ci_string_view> allowed_operators;

    std::string include_path;

    enum class RunType : std::uint8_t
    {
        // Just parse and get the normalised model.
        NoDetection,
        // Detect and save all mutants.
        FullRun
    };

    RunType run_type = RunType::FullRun;
};

struct retrieve_mutants_args
{
    const std::filesystem::path& model_path;
    const std::filesystem::path& directory_path;

    std::span<const ascii_ci_string_view> allowed_operators;
    std::span<const ascii_ci_string_view> allowed_mutants;

    bool check_model_last_modified_time;
};

struct run_mutants_args
{
    EntryResult& entry_result;

    const std::filesystem::path& compiler_path;
    std::span<const std::string_view> compiler_arguments;

    std::span<const ascii_ci_string_view> allowed_mutants;
    std::span<const std::string> data_files;

    std::chrono::seconds timeout;

    std::uint64_t n_jobs;

    bool check_compiler_version;

    logging::output output_log;
};

[[nodiscard]] std::filesystem::path get_path_from_model_path(const std::filesystem::path& model_path);

[[nodiscard]] EntryResult find_mutants(const find_mutants_args& parameters);

[[nodiscard]] EntryResult retrieve_mutants(const retrieve_mutants_args& parameters);
void dump_mutants(const EntryResult& entries, const std::filesystem::path& directory);

void run_mutants(const run_mutants_args& parameters);

void clear_mutant_output_folder(const std::filesystem::path& model_path, const std::filesystem::path& output_directory);

[[nodiscard]] constexpr std::string_view get_version() noexcept { return MZN_VERSION; }
[[nodiscard]] constexpr std::string_view get_version_full() noexcept { return MZN_VERSION_FULL; }

} // namespace MuMiniZinc

#endif