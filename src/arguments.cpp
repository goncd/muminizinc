#include <algorithm>   // std::ranges::find_if
#include <array>       // std::array
#include <cstdlib>     // EXIT_SUCCESS
#include <iostream>    // std::cerr
#include <print>       // std::println
#include <ranges>      // std::views::filter
#include <span>        // std::span
#include <string_view> // std::string_view

#include <minizinc/config.hh> // MZN_VERSION_MAJOR, MZN_VERSION_MINOR, MZN_VERSION_PATCH
#include <minizinc/solver.hh> // Minizinc::MznSolver
#include <minizinc/timer.hh>  // Minizinc::Timer

#include <arguments.hpp>
#include <mutation.hpp> // MutationModel

using namespace std::string_view_literals;

using command_pointer = int (*)(std::span<const char*>);

struct Command
{
    const std::string_view name;
    const std::string_view short_name;
    const std::string_view help;
    const command_pointer operation;

    [[nodiscard]] constexpr bool is_option() const noexcept { return name.starts_with("--"); };
};

namespace
{

int print_version(std::span<const char*>)
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

int run(std::span<const char*> arguments)
{
    /* if (arguments.empty())
     {
         std::println(std::cerr, "analyse: No .mzn found");

         return EXIT_FAILURE;
     }*/

    const MiniZinc::Timer startTime;

    MiniZinc::MznSolver slv(std::cout, std::cerr, startTime);

    std::vector<std::string> v;

    for (const auto* const i : arguments)
        v.emplace_back(i);

    slv.run(v, "");

    return EXIT_SUCCESS;
}

int print_help(std::span<const char*> arguments);
int help(std::span<const char*> arguments);
int clean(std::span<const char*> arguments);

constexpr std::array commands {
    Command {
        .name = "analyse",
        .short_name = {},
        .help = "Analyses the given MiniZinc program",
        .operation = analyse },
    Command {
        .name = "run",
        .short_name = {},
        .help = "Runs all the mutants",
        .operation = run },
    Command {
        .name = "clean",
        .short_name = {},
        .help = "Cleans the working directory for a file",
        .operation = clean },
    Command {
        .name = "help",
        .short_name = {},
        .help = "Print this message or the help of the given subcommand",
        .operation = help },
    Command {
        .name = "--help",
        .short_name = "-h",
        .help = "Print this message or the help of the given subcommand",
        .operation = print_help },
    Command {
        .name = "--version",
        .short_name = "-v",
        .help = "Prints the version",
        .operation = print_version }
};

int print_help(std::span<const char*>)
{
    std::println("MuMiniZinc is a mutation test tool for MiniZinc programs.");

    std::println("\nUsage: ./muminizinc [COMMAND]\n\nCommands:");

    for (const auto& command : commands | std::views::filter([](const auto& command)
                                   { return !command.is_option(); }))
        std::println("\t{}\t\t{}", command.name, command.help);

    std::println("\nOptions:");
    for (const auto& option : commands | std::views::filter([](const auto& option)
                                  { return option.is_option(); }))
        std::println("\t{}, {}\t{}", option.short_name, option.name, option.help);

    return EXIT_SUCCESS;
}

int help(std::span<const char*> arguments)
{
    if (arguments.empty())
        return print_help(arguments);

    if (arguments.size() > 1)
    {
        std::println(std::cerr, "Error: Too many arguments for command `help`");
        return EXIT_FAILURE;
    }

    const auto* const command = std::ranges::find_if(commands, [argument = arguments.front()](const auto& command)
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

int clean(std::span<const char*> arguments)
{
    if (arguments.empty())
    {
        std::println(std::cerr, "analyse: No .mzn found");

        return EXIT_FAILURE;
    }

    // TODO

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
            if (arg == command.name || arg == command.short_name)
                // The command's options will be handled by itself.
                return command.operation(args.subspan(i + 1));

        // If we have reached this point, we have found an argument that does not match
        // our commands or our global arguments.
        std::println(std::cerr, "Error: Unknown command or option `{:s}`", arg);

        return EXIT_FAILURE;
    }

    return print_help(args);
}