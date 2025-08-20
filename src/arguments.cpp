#include <algorithm> // std::ranges::find_if
#include <array>     // std::array
#include <cstdlib>   // EXIT_SUCCESS
#include <filesystem>
#include <format>      // std::format
#include <iostream>    // std::cerr
#include <print>       // std::println
#include <ranges>      // std::views::filter
#include <span>        // std::span
#include <stdexcept>   //
#include <string_view> // std::string_view

#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/process/process.hpp>
#include <boost/process/v2/environment.hpp>
#include <boost/process/v2/stdio.hpp>

#include <minizinc/config.hh> // MZN_VERSION_MAJOR, MZN_VERSION_MINOR, MZN_VERSION_PATCH
#include <minizinc/solver.hh> // Minizinc::MznSolver
#include <minizinc/timer.hh>  // Minizinc::Timer

#include <arguments.hpp>
#include <mutation.hpp> // MutationModel

using namespace std::string_view_literals;

using command_pointer = int (*)(std::span<const char*>);

struct Option
{
    const std::string_view name;
    const std::string_view short_name;
    const std::string_view help;
};

struct Command
{
    Option option;
    const command_pointer operation;
    std::span<const Option> options;

    [[nodiscard]] constexpr bool is_option() const noexcept { return option.name.starts_with("--"); };
};

namespace
{

[[nodiscard]] constexpr auto operator==(const Option& option, std::string_view rhs) noexcept
{
    return option.name == rhs || option.short_name == rhs;
}

int analyse(std::span<const char*> arguments);
int run(std::span<const char*> arguments);
int clean(std::span<const char*> arguments);
int help_subcommand(std::span<const char*> arguments);
int help_subcommand(std::string_view subcommand);
int print_help(std::span<const char*> arguments);
int print_version(std::span<const char*> arguments);

constexpr Option option_directory {
    .name = "--directory",
    .short_name = "-d",
    .help = "Override the auto-generated directory for the model"
};

constexpr Option option_help {
    .name = "--help",
    .short_name = "-h",
    .help = "Print this message or the help of the given subcommand",
};

constexpr Option option_compiler_path {
    .name = "--compiler-path",
    .short_name = "-c",
    .help = "The path of the MiniZinc compiler. By default it's `minizinc`"
};

constexpr std::array analyse_parameters {
    option_directory,
    option_help
};

constexpr std::array run_parameters {
    option_directory,
    option_compiler_path,
    option_help
};

constexpr std::array clean_parameters {
    option_directory,
    option_help
};

constexpr std::array commands {
    Command {
        .option {
            .name = "analyse",
            .short_name = {},
            .help = "Analyses the given MiniZinc program" },
        .operation = analyse,
        .options = analyse_parameters },
    Command {
        .option {
            .name = "run",
            .short_name = {},
            .help = "Runs all the mutants" },
        .operation = run,
        .options = run_parameters },
    Command {
        .option {
            .name = "clean",
            .short_name = {},
            .help = "Cleans the working directory for a model" },
        .operation = clean,
        .options = clean_parameters },
    Command {
        .option {
            .name = "help",
            .short_name = {},
            .help = "Print this message or the help of the given subcommand" },
        .operation = help_subcommand,
        .options = {} },
    Command {
        .option = option_help,
        .operation = print_help,
        .options = {} },
    Command {
        .option {
            .name = "--version",
            .short_name = "-v",
            .help = "Prints the version" },
        .operation = print_version,
        .options = {} }
};

int analyse(std::span<const char*> arguments)
{
    if (arguments.empty())
        throw std::runtime_error { "analyse: At least one parameter is required." };

    std::string_view output_directory {};

    for (std::size_t i { 0 }; i < arguments.size(); ++i)
    {
        const std::string_view argument { arguments[i] };

        if (argument == option_directory)
        {
            if (i + 1 < arguments.size())
            {
                output_directory = arguments[i + 1];
                ++i;
            }
            else
                throw std::runtime_error { "analyse: --directory: Missing parameter." };
        }
        else if (argument == option_help)
            return help_subcommand("analyse"sv);
        else if (i != 0)
            throw std::runtime_error { std::format(R"(analyse: Unknown parameter "{:s}".)", arguments[i]) };
    }

    MutationModel model { std::filesystem::path { arguments.front() }, output_directory };

    model.find_mutants();

    return EXIT_SUCCESS;
}

int run(std::span<const char*> arguments)
{
    if (arguments.empty())
    {
        std::println(std::cerr, "run: At least one parameter is required.");

        return EXIT_FAILURE;
    }

    std::string_view output_directory {};
    std::string_view compiler_path { "minizinc" };
    std::span<const char*> remaining_args {};

    for (std::size_t i { 0 }; i < arguments.size(); ++i)
    {
        const std::string_view argument { arguments[i] };

        if (argument == option_directory)
        {
            if (i + 1 < arguments.size())
            {
                output_directory = arguments[i + 1];
                ++i;
            }
            else
                throw std::runtime_error { "run: --directory: Missing parameter." };
        }
        else if (argument == option_help)
            return help_subcommand("run"sv);
        else if (argument == option_compiler_path)
        {
            if (i + 1 < arguments.size())
            {
                compiler_path = arguments[i + 1];
                ++i;
            }
            else
                throw std::runtime_error { "run: --compiler_path: Missing parameter." };
        }
        else if (argument == "--"sv)
        {
            remaining_args = arguments.subspan(i + 1);

            break;
        }
        else if (i != 0)
            throw std::runtime_error { std::format(R"(run: Unknown parameter "{:s}". Tip: If you want to pass arguments to the compiler, put `--` before them.)", arguments[i]) };
    }

    boost::asio::io_context ctx;

    boost::asio::readable_pipe out_pipe { ctx };
    boost::asio::readable_pipe err_pipe { ctx };
    std::string output_out;
    std::string output_err;

    std::vector<boost::string_view> program_arguments;
    program_arguments.reserve(remaining_args.size());

    for (const auto* const argument : remaining_args)
        program_arguments.emplace_back(argument);

    const boost::filesystem::path executable_from_user { compiler_path };

    const auto executable = boost::filesystem::exists(executable_from_user) ? executable_from_user : boost::process::environment::find_executable(executable_from_user);

    if (executable.empty())
        throw std::runtime_error { std::format("Could not find the executable `{:s}`. Please add it to $PATH.", executable.c_str()) };

    boost::process::process proc(ctx,
        executable,
        program_arguments, boost::process::process_stdio { .in = nullptr, .out = out_pipe, .err = err_pipe });

    boost::system::error_code ec;
    boost::asio::read(out_pipe, boost::asio::dynamic_buffer(output_out), ec);
    assert(!ec || (ec == asio::error::eof));

    boost::asio::read(err_pipe, boost::asio::dynamic_buffer(output_err), ec);
    assert(!ec || (ec == asio::error::eof));

    const auto status = proc.wait();

    std::println("Exit status: {}", status);

    if (status == 0)
        std::println("Out:\n{}", output_out);
    else
        std::println("Error:\n{}", output_err);

    return status;
}

int clean(std::span<const char*> arguments)
{
    if (arguments.empty())
    {
        std::println(std::cerr, "analyse: No .mzn found");

        return EXIT_FAILURE;
    }

    std::string_view output_directory {};

    for (std::size_t i { 0 }; i < arguments.size(); ++i)
    {
        const std::string_view argument { arguments[i] };

        if (argument == option_directory)
        {
            if (i + 1 < arguments.size())
            {
                output_directory = arguments[i + 1];
                ++i;
            }
            else
                throw std::runtime_error { "clean: --directory: Missing parameter." };
        }
        else if (argument == option_help)
            return help_subcommand("clean"sv);
        else if (i != 0)
            throw std::runtime_error { std::format(R"(clean: Unknown parameter "{:s}".)", arguments[i]) };
    }

    MutationModel model { std::filesystem::path { arguments.front() }, output_directory };

    model.clear_output_folder();

    return EXIT_SUCCESS;
}

int help_subcommand(std::span<const char*> arguments)
{
    if (arguments.empty() || (arguments.size() == 2 && arguments[1] == option_help))
        return print_help(arguments);

    if (arguments.size() > 1)
    {
        std::println(std::cerr, "Error: Too many arguments for command `help`.");
        return EXIT_FAILURE;
    }

    return help_subcommand(arguments.front());
}

int help_subcommand(std::string_view subcommand)
{
    const auto* const command = std::ranges::find_if(commands, [subcommand](const auto& command)
        { return command.option.name == subcommand; });

    if (command == commands.end())
    {
        std::println(std::cerr, "Error: help: Unknown command `{:s}`.", subcommand);

        return EXIT_FAILURE;
    }

    std::println("{}", command->option.help);

    std::println("\nUsage: ./minizinc {} <MODEL> <ARGUMENTS>", command->option.name);

    if (!command->options.empty())
    {
        std::println("\nOptions:");
        for (const auto& option : command->options)
            std::println("  {}, {:<15}\t{:<25}", option.short_name, option.name, option.help);
    }

    return EXIT_SUCCESS;
}

int print_help(std::span<const char*>)
{
    std::println("MuMiniZinc is a mutation test tool for MiniZinc programs.");

    std::println("\nUsage: ./muminizinc [COMMAND]\n\nCommands:");

    for (const auto& command : commands | std::views::filter([](const auto& command)
                                   { return !command.is_option(); }))
        std::println("  {:<10}{:<20}", command.option.name, command.option.help);

    std::println("\nOptions:");
    for (const auto& option : commands | std::views::filter([](const auto& option)
                                  { return option.is_option(); }))
        std::println("  {}, {:<15}{:<25}", option.option.short_name, option.option.name, option.option.help);

    return EXIT_SUCCESS;
}

int print_version(std::span<const char*>)
{
    std::println("Built with MiniZinc {:s}.{:s}.{:s}", MZN_VERSION_MAJOR, MZN_VERSION_MINOR, MZN_VERSION_PATCH);

    return EXIT_SUCCESS;
}

}

int parse_arguments(std::span<const char*> args)
{
    if (args.size() < 2)
        return print_help(args);

    for (std::size_t i { 1 }; i < args.size(); ++i)
    {
        const auto arg = std::string_view { args[i] };

        for (const auto& command : commands)
            if (arg == command.option)
                // The command's options will be handled by itself.
                return command.operation(args.subspan(i + 1));

        // If we have reached this point, we have found an argument that does not match
        // our commands or our global arguments.
        std::println(std::cerr, "Error: Unknown command or option `{:s}`.", arg);

        return EXIT_FAILURE;
    }

    return print_help(args);
}