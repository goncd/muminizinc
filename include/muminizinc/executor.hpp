#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

#include <chrono>      // std::chrono::seconds
#include <exception>   // std::runtime_error
#include <filesystem>  // std::filesystem::path
#include <span>        // std::span
#include <string>      // std::string
#include <string_view> // std::string_view

#include <muminizinc/case_insensitive_string.hpp> // ascii_ci_string_view
#include <muminizinc/logging.hpp>                 // logging::output
#include <muminizinc/mutation.hpp>                // MuMiniZinc::Entry

/**
 * @file
 * @brief Execution of mutants.
 */
namespace MuMiniZinc
{

class EntryResult;

/** Arguments for the MuMiniZinc::execute_mutants function. */
struct execution_args
{
    /** The path of the compiler. */
    const std::filesystem::path& compiler_path;

    /**
     * The arguments that will be passed to the compiler. More
     * arguments will be injected alongside these.
     */
    std::span<const std::string_view> compiler_arguments;

    /** The paths of the data files, which will be passed to the compiler as arguments. */
    std::span<const std::string> data_files;

    /** The entries that will be filled with the results of the execution. */
    std::span<Entry> entries;

    /**
     * The normalized model, which will be executed.
     * Its output will be compared against the mutants' output.
     **/
    std::string_view normalized_model;

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
     * The list of allowed mutants. If empty, all mutants will be executed.
     * If any of them cannot be found, \ref UnknownMutant will be thrown.
     */
    std::span<const ascii_ci_string_view> allowed_mutants;

    /**
     * Checks that the compiler version matches the version of MiniZinc
     * used for compiling this project.
     *
     * Causes \ref BadVersion to be thrown if not found.
     */
    bool check_compiler_version;

    /** Where to output the progress. */
    logging::output output_log;
};

/**
 * Thrown when the compilers' version does not match the version of MiniZinc
 * used for compiling this project.
 */
class BadVersion : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/** Thrown if an unknown mutant is specified through MuMiniZinc::execution_args::allowed_mutants */
class UnknownMutant : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/**
 * Thrown if the output cannot be grabbed from any execution or if the original model cannot be run.
 */
class ExecutionError : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/**
 * Runs the original model, then runs the mutants and compare their outputs with the originals' output.
 *
 * @param parameters The execution parameters
 *
 * @throws BadVersion If the reported compiler version (passing `--version`) does not match the version of MiniZinc used for compiling this project.
            Use MuMiniZinc::execution_args::check_compiler_version for toggling this functionality.
 * @throws UnknownMutant If a specified mutant (from MuMiniZinc::execution_args::allowed_mutants) cannot be found.
 * @throws ExecutionError If the output cannot be grabbed from any execution or if the original model cannot be run.
 */
void execute_mutants(const execution_args& parameters);

} // namespace MuMiniZinc

#endif