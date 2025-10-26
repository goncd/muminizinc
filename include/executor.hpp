#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

#include <chrono>      // std::chrono::seconds
#include <exception>   // std::runtime_error
#include <filesystem>  // std::filesystem::path
#include <span>        // std::span
#include <string>      // std::string
#include <string_view> // std::string_view

#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <logging.hpp>                 // logging::output
#include <mutation.hpp>                // MuMiniZinc::Entry

namespace MuMiniZinc
{

class EntryResult;

struct execution_args
{
    const std::filesystem::path& compiler_path;
    std::span<const std::string_view> compiler_arguments;
    std::span<const std::string> data_files;

    std::span<Entry> entries;
    std::string_view normalized_model;

    std::chrono::seconds timeout;
    std::uint64_t n_jobs;
    std::span<const ascii_ci_string_view> allowed_mutants;
    bool check_compiler_version;
    logging::output output_log;
};

class BadVersion : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class UnknownMutant : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class ExecutionError : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

void execute_mutants(const execution_args& parameters);

} // namespace MuMiniZinc

#endif