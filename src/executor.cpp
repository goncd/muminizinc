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

#include <logging.hpp>  // logging::code, logging::Style
#include <mutation.hpp> // MutationModel::Entry

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
    std::string_view original_output;
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
            throw std::runtime_error { "Cannot write the input." };
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

            if (error_code && !(error_code == boost::asio::error::eof))
                throw std::runtime_error { "Cannot grab the output of the executable." };

            if constexpr(std::is_same_v<Job, OriginalJob>)
            {
                if (exit_code != EXIT_SUCCESS)
                {
                    std::println(); // Print a new line so the exception message is below the progress text.
                    throw std::runtime_error { std::format("Could not run the original model:\n{:s}", output) };
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

}

void execute_mutants(const std::filesystem::path& path, std::span<const std::string_view> compiler_arguments, std::span<const std::string> data_files, std::span<MutationModel::Entry> models, std::chrono::seconds timeout, std::uint64_t n_jobs)
{
    if (models.empty())
        return;

    // Set the arguments for the executable.
    std::vector<boost::string_view> arguments;
    arguments.reserve(compiler_arguments.size() + (data_files.empty() ? 1 : 2));

    arguments.emplace_back("-");

    for (const auto argument : compiler_arguments)
        arguments.emplace_back(argument.begin(), argument.size());

    // Handle the user-given timeout.
    std::string time_limit;

    if (timeout != std::chrono::seconds::zero())
    {
        time_limit = std::to_string(timeout / std::chrono::milliseconds { 1 });

        arguments.emplace_back("--time-limit");
        arguments.emplace_back(time_limit);
    }

    if (!data_files.empty())
        arguments.emplace_back();

    std::vector<std::string> original_outputs { std::max(data_files.size(), std::vector<std::string>::size_type { 1 }) };

    // First, just run the original model, so we can make sure it actually compiles and runs with all the provided data files.
    models.front().results.resize(original_outputs.size(), MutationModel::Entry::Status::Alive);

    boost::asio::io_context ctx;

    std::queue<OriginalJob> original_jobs;

    if (data_files.empty())
        original_jobs.emplace(models.front().contents, std::string_view {}, original_outputs.front());
    else
    {
        for (const auto [index, data_file] : std::ranges::views::enumerate(data_files))
            original_jobs.emplace(models.front().contents, data_file, original_outputs[static_cast<std::size_t>(index)]);
    }

    const double total_tasks { static_cast<double>(models.size() * original_outputs.size()) };

    std::uint64_t completed_tasks {};

    for (std::size_t i {}; (n_jobs == 0 || i < n_jobs) && !original_jobs.empty(); ++i)
        launch_process(ctx, path, timeout, original_jobs, arguments, completed_tasks, total_tasks);

    ctx.run();

    // Now, add all the mutants with all the data files and compare their outputs against the original model.
    std::queue<MutantJob> mutant_jobs;

    for (auto& mutant : models | std::ranges::views::drop(1))
    {
        mutant.results.resize(original_outputs.size(), MutationModel::Entry::Status::Alive);

        if (data_files.empty())
            mutant_jobs.emplace(mutant.contents, std::string_view {}, original_outputs.front(), mutant.results.front());
        else
        {
            for (const auto [index, data_file] : std::ranges::views::enumerate(data_files))
            {
                const auto index_value = static_cast<std::size_t>(index);
                mutant_jobs.emplace(mutant.contents, data_file, original_outputs[index_value], mutant.results[index_value]);
            }
        }
    }

    for (std::size_t i {}; (n_jobs == 0 || i < n_jobs) && !mutant_jobs.empty(); ++i)
        launch_process(ctx, path, timeout, mutant_jobs, arguments, completed_tasks, total_tasks);

    ctx.restart();
    ctx.run();

    std::print("\n\n");
}