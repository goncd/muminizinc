#include <algorithm>   // std::ranges::find_if
#include <array>       // std::array
#include <cstdlib>     // EXIT_SUCCESS
#include <iostream>    // std::cerr
#include <print>       // std::println
#include <span>        // std::span
#include <string_view> // std::string_view

#include <minizinc/config.hh> // MZN_VERSION_MAJOR, MZN_VERSION_MINOR, MZN_VERSION_PATCH

#include <arguments.hpp>
#include <mutation.hpp> // MutationModel

using namespace std::string_view_literals;

using command_pointer = int (*)(std::span<const char*>);

struct command
{
    std::string_view name;
    std::string_view help;
    command_pointer operation;
};

struct option
{
    std::string_view name;
    std::string_view short_name;
    std::string_view help;
};

namespace
{

int print_version() noexcept
{
    std::println("Built with MiniZinc {:s}.{:s}.{:s}", MZN_VERSION_MAJOR, MZN_VERSION_MINOR, MZN_VERSION_PATCH);

    return EXIT_SUCCESS;
}

int analyse(std::span<const char*> arguments)
{
    if (arguments.empty())
    {
        std::println(std::cerr, "analyse: No .mzn found");

        return EXIT_FAILURE;
    }

    MutationModel model { arguments.front() };

    model.find_mutants();

    return EXIT_SUCCESS;
}

int help(std::span<const char*> arguments);

static constexpr std::array commands {
    command {
        .name = "analyse",
        .help = "Analyses the given MiniZinc program",
        .operation = analyse },
    command {
        .name = "help",
        .help = "Print this message or the help of the given subcommand",
        .operation = help }
};

static constexpr std::array global_options {
    option {
        .name = "--help",
        .short_name = "-h",
        .help = "Prints this help message" },
    option {
        .name = "--version",
        .short_name = "-v",
        .help = "Prints the version" }
};

int print_help() noexcept
{
    std::println("MuMiniZinc is a mutation test tool for MiniZinc programs.");

    std::println("\nUsage: ./muminizinc [COMMAND]\n\nCommands:");

    for (const auto& command : commands)
        std::println("\t{}\t\t{}", command.name, command.help);

    std::println("\nOptions:");
    for (const auto& option : global_options)
        std::println("\t{}, {}\t{}", option.short_name, option.name, option.help);

    return EXIT_SUCCESS;
}

int help(std::span<const char*> arguments)
{
    if (arguments.empty())
        return print_help();

    if (arguments.size() > 1)
    {
        std::println(std::cerr, "Error: Too many arguments for command `help`");
        return EXIT_FAILURE;
    }

    const auto command = std::ranges::find_if(commands, [argument = arguments.front()](const auto& command)
        { return command.name == argument; });

    if (command == commands.end())
    {
        std::println(std::cerr, "Error: Unknown command `{:s}`", arguments.front());

        return EXIT_FAILURE;
    }

    std::println("{}", command->help);

    std::println("\nUsage: ./minizinc {} <options>", command->name);

    return EXIT_SUCCESS;
}

}

int parse_arguments(std::span<const char*> args)
{
    if (args.size() < 2)
        return print_help();

    for (std::size_t i { 1 }; i < args.size(); ++i)
    {
        const auto arg = std::string_view { args[i] };

        // First, determine if it's a global option. If we find a global option that does not return,
        // then we'll end here.
        if (arg.starts_with('-') || arg.starts_with("--"))
        {
            for (const auto& option : global_options)
            {
                if (arg == option.short_name || arg == option.name)
                {
                }
            }
        }

        for (const auto& command : commands)
            if (arg == command.name)
                // The command's options will be handled by itself.
                return command.operation(args.subspan(i + 1));

        // If we have reached this point, we have found an argument that does not match
        // our commands or our global arguments.
        std::println(std::cerr, "Error: Unknown command or option `{:s}`", arg);

        return EXIT_FAILURE;
    }

    return print_help();
}