#ifndef MUTATION_HPP
#define MUTATION_HPP

#include <array>       // std::array
#include <filesystem>  // std::filesystem::path
#include <functional>  // std::reference_wrapper
#include <span>        // std::span
#include <string>      // std::string
#include <string_view> // std::string_view
#include <utility>     // std::move, std::reference_wrapper
#include <variant>     // std::variant
#include <vector>      // std::vector

#include <minizinc/config.hh> // MZN_VERSION_MAJOR, MZN_VERSION_MINOR, MZN_VERSION_PATCH
#include <minizinc/model.hh>  // MiniZinc::Model

#include <muminizinc/case_insensitive_string.hpp> // ascii_ci_string_view
#include <muminizinc/logging.hpp>                 // logging::output
#include <muminizinc/operators.hpp>               // MuMiniZinc::available_operators

/**
 * @file
 * @brief The main interface of the library.
 */
namespace MuMiniZinc
{

/** Thrown when a mutant that is older than the original model is detected. */
class OutdatedMutant : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/** General IO exception. */
class IOError : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/** Thrown when an empty filed is detected while finding or retrieving mutants. */
class EmptyFile : public IOError
{
    using IOError::IOError;
};

/** Thrown when a file that is not a mutant or a normalized model is detected. */
class InvalidFile : public IOError
{
    using IOError::IOError;
};

/** Thrown when an operator cannot be found. */
class UnknownOperator : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class EntryResult;
struct find_mutants_args;
struct retrieve_mutants_args;
struct run_mutants_args;

/** Represents a mutant and its properties. */
struct Entry
{
    /**
     * Constructs an Entry.
     *
     * @param name the name of the mutant.
     * @param contents the contents of the mutant.
     */
    constexpr Entry(std::string name, std::string contents) noexcept :
        name { std::move(name) }, contents { std::move(contents) } { }

    constexpr Entry() noexcept = default;

    /** Default three-way comparison. */
    [[nodiscard]] constexpr auto operator<=>(const Entry&) const noexcept = default;

    /**
     * The result of this entry compared to the original mutant.
     * This is filled by MuMiniZinc::execute_mutants.
     */
    enum class Status : std::uint8_t
    {
        /**
         * The result of the execution of the mutant is the exact same as the
         * original.
         */
        Alive,
        /**
         * The result of the execution of the mutant is different to the original.
         * The origin of this difference may be because it timed out.
         */
        Dead,
        /** An error had occurred when executing this mutant. */
        Invalid
    };

    /** The results of the mutant tested against every data file. */
    std::vector<Status> results;

    /** The name of the mutant. */
    std::string name;

    /** The contents of the mutant. */
    std::string contents;
};

/** Represents the results of an analysis of a model and the execution of it and its mutants. */
class EntryResult
{
    std::vector<Entry> m_mutants;
    std::array<std::pair<std::uint64_t, std::uint64_t>, available_operators.size()> m_statistics {};

    std::string m_model_name;
    std::string m_model_contents;

    friend EntryResult find_mutants(const find_mutants_args& parameters);
    friend EntryResult retrieve_mutants(const retrieve_mutants_args& parameters);
    friend void run_mutants(const run_mutants_args& parameters);
    friend class Mutator;

    void save_model(const MiniZinc::Model* model, std::string_view operator_name, std::uint64_t location_id, std::uint64_t occurrence_id, std::span<const std::pair<std::string, std::string>> detected_enums);

public:
    /** Default three-way comparison. */
    [[nodiscard]] constexpr auto operator<=>(const EntryResult&) const noexcept = default;

    /** The stored mutants. */
    [[nodiscard]] constexpr std::span<const Entry> mutants() const noexcept { return m_mutants; }

    /** The name of the model. */
    [[nodiscard]] constexpr std::string_view model_name() const noexcept { return m_model_name; }

    /** The normalized original model. */
    [[nodiscard]] constexpr std::string_view normalized_model() const noexcept { return m_model_contents; }

    /**
     * The statistics corresponding to the present operators. Each element of the returned span corresponds to
     * an operator, in the same order as in MuMiniZinc::available_operators. The returned span has the same size as
     * the mentioned array.
     *
     * The first parameter is the amount of mutants generated with the operator and the second parameter
     * is the amount of different types of mutants that have been generated with the operator.
     */
    [[nodiscard]] constexpr std::span<const std::pair<std::uint64_t, std::uint64_t>> statistics() const noexcept { return m_statistics; }
};

/** Arguments for the MuMiniZinc::find_mutants function. */
struct find_mutants_args
{
    /** The details of the model to analyze. */
    struct ModelDetails
    {
        /** The name of the original model. */
        std::string name;

        /** The contents of the original model. */
        std::string contents;
    };

    /**
     * The source for the model. It can be either a string (a ModelDetails) or a path to a model.
     */
    std::variant<ModelDetails, std::reference_wrapper<const std::filesystem::path>> model;

    /** The allowed operators, from MuMiniZinc::available_operators */

    /**
     * The list of the operators' short names allowed to generate mutants,
     * from MuMiniZinc::available_operators.
     *
     * If any of these operators couldn't be found, \ref UnknownOperator will be thrown.
     */
    std::span<const ascii_ci_string_view> allowed_operators;

    /** The include path, given to the MiniZinc parser. */
    std::string include_path;

    /** The run types. */
    enum class RunType : std::uint8_t
    {
        /** Just parse and get the normalised model. */
        NoDetection,
        /* Detect and save all mutants. */
        FullRun
    };

    /** The run type, defaults to full run. */
    RunType run_type = RunType::FullRun;
};

/** Arguments for the MuMiniZinc::retrieve_mutants function. */
struct retrieve_mutants_args
{
    /** The path to the model. */
    std::reference_wrapper<const std::filesystem::path> model_path;

    /** The path to the directory that should have the mutants. */
    std::reference_wrapper<const std::filesystem::path> directory_path;

    /** A list of the allowed operators to retrieve. */
    std::span<const ascii_ci_string_view> allowed_operators;

    /** A list of the allowed mutants to retrieve. */
    std::span<const ascii_ci_string_view> allowed_mutants;

    /** Whether to check if the mutants are older than the original model. */
    bool check_model_last_modified_time;
};

/** Arguments for the MuMiniZinc::run_mutants function. */
struct run_mutants_args
{
    /** A reference to the MuMiniZinc::EntryResult to dump the results to. */
    EntryResult& entry_result;

    /** The path to the compiler that will be used for executing the model and the mutants. */
    std::reference_wrapper<const std::filesystem::path> compiler_path;

    /** The arguments that will be passed to the compiler. */
    std::span<const std::string_view> compiler_arguments;

    /**
     * The list of allowed mutants to run.
     *
     * If any of these mutants couldn't be found, \ref UnknownMutant will be thrown.
     */
    std::span<const ascii_ci_string_view> allowed_mutants;

    /** The paths to the data files that will be used for running the model and the mutants. */
    std::span<const std::string> data_files;

    /**
     * The amount of time that should be waited before treating it as a dead mutant.
     * If the original model timeouts, \ref ExecutionError will be thrown.
     *
     * If zero, then there will be no timeout.
     */
    std::chrono::seconds timeout;

    /**
     * The maximum number of concurrent compiler executions. If zero, the execution will be
     * single-threaded.
     *
     * The retrival of the output and mutant comparison is always single-threaded.
     */
    std::uint64_t n_jobs;

    /**
     * Checks that the compiler version matches the version of MiniZinc
     * used for compiling this project.
     *
     * Causes \ref BadVersion to be thrown if the version does not match.
     */
    bool check_compiler_version;

    /** Where to output the progress. */
    logging::output output_log;
};

/**
 * Computes and returns the default path for the mutants folder for the given model path.
 *
 * @param model_path the path to the model.
 *
 * @return the path to the mutants folder.
 *
 * @throw IOError if the path cannot be determined.
 */
[[nodiscard]] std::filesystem::path get_path_from_model_path(const std::filesystem::path& model_path);

/**
 * Analyzes the given model and normalizes it. If instructed, it finds and generates
 * the corresponding mutants.
 *
 * @param parameters the parameters.
 *
 * @return an EntryResult.
 *
 * @throw MiniZinc::Exception if there is a parsing error.
 * @throw IOError if there is an error when trying to open the model file, if a path was provided.
 * @throw EmptyFile if the given model is empty.
 * @throw UnknownOperator if an operator is given.
 */
[[nodiscard]] EntryResult find_mutants(const find_mutants_args& parameters);

/**
 * Retrieves the mutants from the filesystem. This will not retrieve the mutation operators' statistics.
 *
 * @param parameters the parameters.
 *
 * @return an `EntryResult`.
 */
[[nodiscard]] EntryResult retrieve_mutants(const retrieve_mutants_args& parameters);

/**
 * Dumps the mutants and the normalized model to the filesystem.
 * This will not dump the mutation operators' statistics.
 *
 * @param entries the mutants and the normalized model to dump.
 * @param directory the directory to dump the mutants to.
 *
 * @throws IOError if the data couldn't be dumped.
 */
void dump_mutants(const EntryResult& entries, const std::filesystem::path& directory);

/**
 * Runs the original model and the mutants and compare their results.
 *
 * @param parameters the parameters.
 *
 * @throws BadVersion if the reported compiler version (passing `--version`) does not match the version of MiniZinc used for compiling this project.
           Use MuMiniZinc::run_mutants_args::check_compiler_version for toggling this functionality.
 * @throws UnknownMutant if a specified mutant (from MuMiniZinc::run_mutants_args::allowed_mutants) cannot be found.
 * @throws ExecutionError if the output cannot be grabbed from any execution or if the original model cannot be run.
 */
void run_mutants(const run_mutants_args& parameters);

/**
 * Deletes the output folder for the specified mutant.
 *
 * @param model_path the path to the original model.
 * @param output_directory the output directory to clear.
 *
 * @throws std::runtime_error if \p output_directory is an empty string.
 * @throws std::filesystem::filesystem_error if the folder couldn't be deleted.
 * @throws InvalidFile if a file that is not a mutant or the normalized model is detected
           inside the folder.
 */
void clear_mutant_output_folder(const std::filesystem::path& model_path, const std::filesystem::path& output_directory);

/** The MiniZinc version used for this project. */
inline constexpr std::string_view minizinc_version { MZN_VERSION_MAJOR "." MZN_VERSION_MINOR "." MZN_VERSION_PATCH };

/** The MiniZinc version used for this project, with "version" appended at the beginning. */
inline constexpr std::string_view minizinc_version_full { "version " MZN_VERSION_MAJOR "." MZN_VERSION_MINOR "." MZN_VERSION_PATCH };

} // namespace MuMiniZinc

#endif