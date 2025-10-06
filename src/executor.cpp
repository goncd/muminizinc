#include <executor.hpp>

#include <algorithm>   // std::max
#include <chrono>      // std::chrono::seconds
#include <cstdint>     // std::uint64_t
#include <cstdlib>     // EXIT_SUCCESS
#include <filesystem>  // std::filesystem::path
#include <format>      // std::format
#include <iostream>    // std::cout
#include <memory>      // std::make_unique
#include <print>       // std::print
#include <queue>       // std::queue
#include <ranges>      // std::views::drop
#include <span>        // std::span
#include <string>      // std::string
#include <string_view> // std::string_view
#include <type_traits> // std::is_same_v
#include <utility>     // std::move
#include <vector>      // std::vector

#include <boost/asio/buffer.hpp>         // boost::asio::dynamic_buffer
#include <boost/asio/error.hpp>          // boost::asio::error::eof
#include <boost/asio/io_context.hpp>     // boost::asio::io_context
#include <boost/asio/read.hpp>           // boost::asio::read
#include <boost/asio/readable_pipe.hpp>  // boost::asio::readable_pipe
#include <boost/asio/writable_pipe.hpp>  // boost::asio::writable_pipe
#include <boost/asio/write.hpp>          // boost::asio::write
#include <boost/process/v2/process.hpp>  // boost::process::process
#include <boost/process/v2/stdio.hpp>    // boost::process::process_stdio
#include <boost/system/error_code.hpp>   // boost::system::error_code
#include <boost/utility/string_view.hpp> // boost::string_view

#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <logging.hpp>                 // logging::code, logging::Style
#include <mutation.hpp>                // MutationModel::Entry

namespace
{

struct OriginalJob
{
    std::string_view contents;
    std::string_view data_file;
    std::string& output;
};

struct MutantJob
{
    std::string_view contents;
    std::string_view data_file;
    std::string& original_output;
    MutationModel::Entry::Status& status;
};

template<typename Job>
    requires std::is_same_v<Job, OriginalJob> || std::is_same_v<Job, MutantJob>
void launch_process(boost::asio::io_context& ctx, const std::filesystem::path& path, std::chrono::seconds timeout, std::queue<Job>& jobs, std::span<boost::string_view> arguments, std::uint64_t& completed_tasks, double total_tasks)
{
    if (jobs.empty())
        return;

    auto job = std::move(jobs.front());
    jobs.pop();

    if (!job.data_file.empty())
        arguments.back() = boost::string_view { job.data_file.begin(), job.data_file.size() };

    boost::asio::readable_pipe out_pipe { ctx };
    boost::asio::readable_pipe err_pipe { ctx };
    boost::asio::writable_pipe in_pipe { ctx };

    auto process = std::make_unique<boost::process::process>(
        ctx,
        path,
        arguments,
        boost::process::process_stdio { .in = in_pipe, .out = out_pipe, .err = err_pipe });

    boost::system::error_code error_code;

    if (!job.contents.empty())
    {
        boost::asio::write(in_pipe, boost::asio::buffer(job.contents), error_code);

        if (error_code && !(error_code == boost::asio::error::eof))
            throw ExecutionError { "Cannot write the input." };
    }

    in_pipe.close();

    process->async_wait([&ctx, &path, timeout, &jobs, arguments, &completed_tasks, total_tasks, out_pipe = std::move(out_pipe), err_pipe = std::move(err_pipe), job = std::move(job), process = std::move(process)](boost::system::error_code ec, int exit_code) mutable
        {
            ++completed_tasks;
            std::print("{:s}{:s}Progress{:s}: {:d} of {:g} execution{:s}({:0.2f}%)", logging::carriage_return(), logging::code(logging::Style::Bold), logging::code(logging::Style::Reset), completed_tasks, total_tasks, total_tasks > 1 ? "s " : " ", static_cast<double>(completed_tasks) / total_tasks * 100);

            if (logging::have_color_output())
                std::cout.flush();
            else
                std::println();

            // If an error occurred, don't do anything.
            if (ec)
                return;

            boost::system::error_code error_code;

            std::string output;

            boost::asio::read(exit_code == EXIT_SUCCESS ? out_pipe : err_pipe, boost::asio::dynamic_buffer(output), error_code);

            if (error_code != boost::asio::error::eof)
                throw ExecutionError { "Cannot grab the output of the executable." };

            if constexpr(std::is_same_v<Job, OriginalJob>)
            {
                if (exit_code != EXIT_SUCCESS)
                {
                    std::println(); // Print a new line so the exception message is below the progress text.
                    throw ExecutionError { std::format("Could not run the original model:\n{:s}", output) };
                }

                job.output = std::move(output);
            }
            else
            {
                if (exit_code != EXIT_SUCCESS)
                    job.status = MutationModel::Entry::Status::Invalid;
                else if (output == job.original_output)
                    job.status = MutationModel::Entry::Status::Alive;
                else
                    job.status = MutationModel::Entry::Status::Dead;
            }

            launch_process(ctx, path, timeout, jobs, arguments,completed_tasks, total_tasks); });
}

void check_version(boost::asio::io_context& ctx, const std::filesystem::path& path)
{
    boost::asio::readable_pipe out_pipe { ctx };

    boost::process::process process {
        ctx,
        path,
        { "--version" },
        boost::process::process_stdio { .in = nullptr, .out = out_pipe, .err = nullptr }
    };

    boost::system::error_code error_code;

    std::string output;

    boost::asio::read(out_pipe, boost::asio::dynamic_buffer(output), error_code);

    if (error_code != boost::asio::error::eof)
        throw BadVersion { "Could not verify the compiler's version: Could not grab the output." };

    process.wait();

    if (process.exit_code() != EXIT_SUCCESS)
        throw BadVersion { "Could not verify the compiler's version: The compiler exit code is not success." };

    if (!output.contains(MutationModel::get_version()))
        throw BadVersion { "Compiler version mismatch." };
}

}

void execute_mutants(const configuration& configuration)
{
    if (configuration.models.empty())
        return;

    for (const auto mutant : configuration.mutants)
    {
        bool found = false;

        for (const auto& entry : configuration.models)
        {
            if (mutant == entry.name)
            {
                found = true;
                break;
            }
        }

        if (!found)
            throw UnknownMutant { std::format("Unknown mutant `{:s}{:s}{:s}`.", logging::code(logging::Color::Blue), mutant, logging::code(logging::Style::Reset)) };
    }

    // Set the arguments for the executable.
    std::vector<boost::string_view> arguments;
    arguments.reserve(configuration.compiler_arguments.size() + (configuration.data_files.empty() ? 1 : 2));

    arguments.emplace_back("-");

    for (const auto argument : configuration.compiler_arguments)
        arguments.emplace_back(argument.begin(), argument.size());

    // Handle the user-given timeout.
    std::string time_limit;

    if (configuration.timeout != std::chrono::seconds::zero())
    {
        time_limit = std::to_string(configuration.timeout / std::chrono::milliseconds { 1 });

        arguments.emplace_back("--time-limit");
        arguments.emplace_back(time_limit);
    }

    if (!configuration.data_files.empty())
        arguments.emplace_back();

    std::vector<std::string> original_outputs { std::max(configuration.data_files.size(), std::vector<std::string>::size_type { 1 }) };

    // First, add the jobs for the original model, so we can make sure it actually compiles and runs with all the provided data files.
    configuration.models.front().results.resize(original_outputs.size(), MutationModel::Entry::Status::Alive);

    std::queue<OriginalJob> original_jobs;

    if (configuration.data_files.empty())
        original_jobs.emplace(configuration.models.front().contents, std::string_view {}, original_outputs.front());
    else
    {
        for (const auto [index, data_file] : std::ranges::views::enumerate(configuration.data_files))
            original_jobs.emplace(configuration.models.front().contents, data_file, original_outputs[static_cast<std::size_t>(index)]);
    }

    // Now, add all the mutants with all the data files and compare their outputs against the original model.
    std::queue<MutantJob> mutant_jobs;

    for (auto& mutant : configuration.models | std::ranges::views::drop(1))
    {
        if (!configuration.mutants.empty() && !std::ranges::contains(configuration.mutants, ascii_ci_string_view { mutant.name }))
            continue;

        mutant.results.resize(original_outputs.size(), MutationModel::Entry::Status::Alive);

        if (configuration.data_files.empty())
            mutant_jobs.emplace(mutant.contents, std::string_view {}, original_outputs.front(), mutant.results.front());
        else
        {
            for (const auto [index, data_file] : std::ranges::views::enumerate(configuration.data_files))
            {
                const auto index_value = static_cast<std::size_t>(index);
                mutant_jobs.emplace(mutant.contents, data_file, original_outputs[index_value], mutant.results[index_value]);
            }
        }
    }

    boost::asio::io_context ctx;

    if (configuration.check_compiler_version)
        check_version(ctx, configuration.path);

    const double total_tasks { static_cast<double>(original_jobs.size() + mutant_jobs.size()) };

    std::uint64_t completed_tasks {};

    for (std::size_t i {}; (configuration.n_jobs == 0 || i < configuration.n_jobs) && !original_jobs.empty(); ++i)
        launch_process(ctx, configuration.path, configuration.timeout, original_jobs, arguments, completed_tasks, total_tasks);

    ctx.run();

    for (std::size_t i {}; (configuration.n_jobs == 0 || i < configuration.n_jobs) && !mutant_jobs.empty(); ++i)
        launch_process(ctx, configuration.path, configuration.timeout, mutant_jobs, arguments, completed_tasks, total_tasks);

    ctx.restart();
    ctx.run();

    std::print("\n\n");
}