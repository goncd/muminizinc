#include <executor.hpp>

#include <algorithm>   // std::max
#include <chrono>      // std::chrono::seconds
#include <cstdint>     // std::uint64_t
#include <cstdlib>     // EXIT_SUCCESS
#include <format>      // std::format
#include <memory>      // std::make_unique
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
#include <boost/asio/steady_timer.hpp>   // boost::asio::steady_timer
#include <boost/asio/writable_pipe.hpp>  // boost::asio::writable_pipe
#include <boost/asio/write.hpp>          // boost::asio::write
#include <boost/filesystem/path.hpp>     // boost::filesystem::path
#include <boost/process/v2/process.hpp>  // boost::process::process
#include <boost/process/v2/stdio.hpp>    // boost::process::process_stdio
#include <boost/system/error_code.hpp>   // boost::system::error_code
#include <boost/utility/string_view.hpp> // boost::string_view

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
void launch_process(boost::asio::io_context& ctx, const boost::filesystem::path& path, std::chrono::seconds timeout, std::queue<Job>& jobs, std::span<boost::string_view> arguments)
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

    auto timer = boost::asio::steady_timer(ctx);

    if (timeout == std::chrono::seconds { 0 })
        timer.expires_at(std::chrono::steady_clock::time_point::max());
    else
        timer.expires_after(timeout);

    auto* const process_ptr = process.get();

    timer.async_wait([&ctx, &path, timeout, &jobs, arguments, process = std::move(process), job](boost::system::error_code ec)
        {
            if (!ec)
            {
                process->terminate();

                if constexpr(std::is_same_v<Job, OriginalJob>) {
                    throw std::runtime_error { "Timeout when trying to run the original model." };
                    static_cast<void>(job); // Silence unused lambda capture on this path.
                } else {
                    job.status = MutationModel::Entry::Status::Dead;
                }

                launch_process(ctx, path, timeout, jobs, arguments);
            } });

    process_ptr->async_wait([&ctx, &path, timeout, &jobs, arguments, timer = std::move(timer), out_pipe = std::move(out_pipe), err_pipe = std::move(err_pipe), job](boost::system::error_code ec, int exit_code) mutable
        {
            // If an error occurred or we hit the timeout, don't do anything.
            if(ec)
                return;

            // Cancel the timer, because we have finished and we no longer care about timeouts.
            timer.cancel();

            boost::system::error_code error_code;

            std::string output;

            boost::asio::read(exit_code == EXIT_SUCCESS ? out_pipe : err_pipe, boost::asio::dynamic_buffer(output), error_code);

            if (error_code && !(error_code == boost::asio::error::eof))
                throw std::runtime_error { "Cannot grab the output of the executable." };

            if constexpr(std::is_same_v<Job, OriginalJob>) {
                if (exit_code != EXIT_SUCCESS)
                    throw std::runtime_error { std::format("Could not run the original model:\n{:s}", output) };

                job.output = std::move(output);
            } else {
                if (exit_code != EXIT_SUCCESS)
                    job.status = MutationModel::Entry::Status::Invalid;
                else if (output == job.original_output)
                    job.status = MutationModel::Entry::Status::Alive;
                else
                    job.status = MutationModel::Entry::Status::Dead;
            }

            launch_process(ctx, path, timeout, jobs, arguments); });
}

}

void execute_mutants(const boost::filesystem::path& path, std::span<const std::string_view> compiler_arguments, std::span<const std::string_view> data_files, std::span<MutationModel::Entry> models, std::chrono::seconds timeout, std::uint64_t n_jobs)
{
    if (models.empty())
        return;

    // Set the arguments for the executable.
    std::vector<boost::string_view> arguments;
    arguments.reserve(compiler_arguments.size() + (data_files.empty() ? 1 : 2));

    arguments.emplace_back("-");

    for (const auto argument : compiler_arguments)
        arguments.emplace_back(argument.begin(), argument.size());

    if (!data_files.empty())
        arguments.emplace_back();

    std::vector<std::string> original_outputs;
    original_outputs.resize(data_files.empty() ? 1 : data_files.size());

    const auto n_data_files { std::max(data_files.size(), 1uz) };

    // First, just run the original model, so we can make sure it actually compiles and runs with all mutants.
    models.front().results.resize(n_data_files, MutationModel::Entry::Status::Alive);

    boost::asio::io_context ctx;

    std::queue<OriginalJob> original_jobs;

    if (data_files.empty())
        original_jobs.emplace(models.front().contents, std::string_view {}, original_outputs.front());
    else
    {
        for (const auto [index, data_file] : std::ranges::views::enumerate(data_files))
            original_jobs.emplace(models.front().contents, data_file, original_outputs[static_cast<std::size_t>(index)]);
    }

    for (std::size_t i { 0 }; (n_jobs == 0 || i < n_jobs) && !original_jobs.empty(); ++i)
        launch_process(ctx, path, timeout, original_jobs, arguments);

    ctx.run();

    std::queue<MutantJob> mutant_jobs;

    // Now, add all the mutants with all the data files and compare their outputs against the original model.
    for (auto& mutant : models | std::ranges::views::drop(1))
    {
        mutant.results.resize(n_data_files, MutationModel::Entry::Status::Alive);

        if (data_files.empty())
            mutant_jobs.emplace(mutant.contents, std::string_view {}, original_outputs.front(), mutant.results.front());
        else
        {
            for (const auto [index, data_file] : std::ranges::views::enumerate(data_files))
            {
                arguments.back() = boost::string_view { data_file.begin(), data_file.size() };

                const auto index_value = static_cast<std::size_t>(index);

                mutant_jobs.emplace(mutant.contents, data_file, original_outputs[index_value], mutant.results[index_value]);
            }
        }
    }

    for (std::size_t i { 0 }; (n_jobs == 0 || i < n_jobs) && !mutant_jobs.empty(); ++i)
        launch_process(ctx, path, timeout, mutant_jobs, arguments);

    if (ctx.stopped())
        ctx.restart();

    ctx.run();
}