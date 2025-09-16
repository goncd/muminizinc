#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

#include <chrono>      // std::chrono::seconds
#include <concepts>    // std::invocable
#include <memory>      // std::make_unique
#include <span>        // std::span
#include <string_view> // std::string_view

#include <boost/asio/io_context.hpp>     // boost::asio::io_context
#include <boost/asio/read.hpp>           // boost::asio::read
#include <boost/asio/readable_pipe.hpp>  // boost::asio::readable_pipe
#include <boost/asio/steady_timer.hpp>   // boost::asio::steady_timer
#include <boost/asio/writable_pipe.hpp>  // boost::asio::writable_pipe
#include <boost/asio/write.hpp>          // boost::asio::write
#include <boost/filesystem/path.hpp>     // boost::filesystem::path
#include <boost/process/v2/process.hpp>  // boost::process::process
#include <boost/process/v2/stdio.hpp>    // boost::process::process_stdio
#include <boost/utility/string_view.hpp> // boost::string_view

template<typename F, typename R, typename... Args>
concept Signature = std::invocable<F, Args...> && std::same_as<std::invoke_result_t<F, Args...>, R>;

class Executor
{
public:
    void add_task(const boost::filesystem::path& m_path, std::span<const boost::string_view> arguments, std::string_view input, std::chrono::seconds timeout, Signature<void, int, std::string> auto&& on_completion, Signature<void> auto&& on_timeout);

    void run();

private:
    boost::asio::io_context m_ctx;
};

void Executor::add_task(const boost::filesystem::path& m_path, std::span<const boost::string_view> arguments, std::string_view input, std::chrono::seconds timeout, Signature<void, int, std::string> auto&& on_completion, Signature<void> auto&& on_timeout)
{
    boost::asio::readable_pipe out_pipe { m_ctx };
    boost::asio::readable_pipe err_pipe { m_ctx };
    boost::asio::writable_pipe in_pipe { m_ctx };

    auto process = std::make_unique<boost::process::process>(
        m_ctx,
        m_path,
        arguments,
        boost::process::process_stdio { .in = in_pipe, .out = out_pipe, .err = err_pipe });

    boost::system::error_code error_code;

    if (!input.empty())
    {
        boost::asio::write(in_pipe, boost::asio::buffer(input), error_code);

        if (error_code && !(error_code == boost::asio::error::eof))
            throw std::runtime_error { "Cannot write the input." };
    }

    in_pipe.close();

    auto timer = boost::asio::steady_timer(m_ctx);

    if (timeout == std::chrono::seconds { 0 })
        timer.expires_at(boost::asio::chrono::steady_clock::time_point::max());
    else
        timer.expires_after(timeout);

    auto* const process_ptr = process.get();

    timer.async_wait([process = std::move(process), on_timeout = std::forward<decltype(on_timeout)>(on_timeout)](boost::system::error_code ec)
        {
            if (!ec)
            {
                process->terminate();
                on_timeout();
            } });

    process_ptr->async_wait([timer = std::move(timer), out_pipe = std::move(out_pipe), err_pipe = std::move(err_pipe), on_completion = std::forward<decltype(on_completion)>(on_completion)](boost::system::error_code ec, int exit_code) mutable
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

            on_completion(exit_code, std::move(output)); });
}

#endif