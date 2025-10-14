#include <arguments.hpp>

#include <algorithm>    // std::ranges::find_if
#include <array>        // std::array
#include <charconv>     // std::from_chars
#include <cstdint>      // std::uint64_t
#include <cstdlib>      // EXIT_SUCCESS
#include <filesystem>   // std::filesystem::exists, std::filesystem::path
#include <format>       // std::format
#include <fstream>      // std::ofstream
#include <functional>   // std::reference_wrapper
#include <iostream>     // std::cout
#include <iterator>     // std::ostreambuf_iterator
#include <optional>     // std::optional
#include <print>        // std::println
#include <ranges>       // std::views::enumerate, std::views::filter, std::views::split, std::ranges::to, std::views::transform
#include <span>         // std::span
#include <sstream>      // std::ostringstream
#include <string>       // std::string
#include <string_view>  // std::string_view
#include <system_error> // std::errc
#include <utility>      // std::to_underlying
#include <variant>      // std::variant
#include <vector>       // std::vector

#include <boost/process/v2/environment.hpp> // boost::process::environment::find_executable

#include <nlohmann/json.hpp> // nlohmann::json

#include <build/config.hpp>            // config::project_version
#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <executor.hpp>                // BadVersion
#include <logging.hpp>                 // logging::code, logging::Color, logging::Style
#include <mutation.hpp>                // MuMiniZinc::clear_mutant_output_folder, MuMiniZinc::EntryResult, MuMiniZinc::find_mutants, MuMiniZinc::find_mutants_args, MuMiniZinc::get_path_from_model_path, MuMiniZinc::run_mutants, MuMiniZinc::run_mutants_args
#include <operators/mutator.hpp>       // MuMiniZinc::available_operators

namespace
{

using command_pointer = int (*)(std::span<const std::string_view>);

using namespace std::string_view_literals;

constexpr auto end_of_options_token { "--"sv };
constexpr auto separator_arguments { ',' };

#define DEFAULT_TIMEOUT_S 10

constexpr std::uint64_t default_n_jobs { 0 }; // Unlimited jobs.

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

    bool is_hidden { false };

    [[nodiscard]] constexpr bool is_option() const noexcept { return option.name.starts_with("--"); };
};

[[nodiscard]] constexpr auto operator==(const Option& option, std::string_view rhs) noexcept
{
    return option.name == rhs || option.short_name == rhs;
}

int applyall(std::span<const std::string_view> arguments);
int analyse(std::span<const std::string_view> arguments);
int run(std::span<const std::string_view> arguments);
int normalise(std::span<const std::string_view> arguments);
int clean(std::span<const std::string_view> arguments);
int help_subcommand(std::span<const std::string_view> arguments);

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
    .short_name = "-p",
    .help = "The path of the MiniZinc compiler. By default it's `minizinc`"
};

constexpr Option option_in_memory {
    .name = "--in-memory",
    .short_name = "-m",
    .help = "Runs the command entirely in memory, without reading or writing files",
};

constexpr Option option_color {
    .name = "--color",
    .short_name = "-c",
    .help = R"(Enables color output with "true" or disables it with "false". By default it's automatic)",
};

constexpr Option option_operator {
    .name = "--operator",
    .short_name = "-r",
    .help = "Only process the selected operator, or a comma-separated list of them"
};

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define DEFAULT_TIMEOUT_STR TOSTRING(DEFAULT_TIMEOUT_S)

constexpr Option option_timeout {
    .name = "--timeout",
    .short_name = "-t",
    .help = "Run timeout in seconds. By default it's " DEFAULT_TIMEOUT_STR " seconds"
};

#undef DEFAULT_TIMEOUT_STR
#undef TOSTRING
#undef STRINGIFY

constexpr Option option_data {
    .name = "--data",
    .short_name = "-z",
    .help = "Test all mutants against the specified data file, or a comma-separated list of them"
};

constexpr Option option_jobs {
    .name = "--jobs",
    .short_name = "-j",
    .help = "The maximum number of concurrent execution jobs. A value of 0 (which is the default) makes it unlimited"
};

constexpr Option option_output {
    .name = "--output",
    .short_name = "-o",
    .help = "The path which the output will be redirected to"
};

constexpr Option option_include {
    .name = "--include",
    .short_name = "-I",
    .help = "The include path. By default it will be searched on the directories above this executable"
};

constexpr Option option_mutant {
    .name = "--mutant",
    .short_name = "-u",
    .help = "Only run the specified mutant, or a comma-separated list of them"
};

constexpr Option option_json {
    .name = "--json",
    .short_name = {},
    .help = "When success, output will be in JSON format"
};

constexpr Option option_ignore_version_check {
    .name = "--ignore-version-check",
    .short_name = {},
    .help = "Ignore the compiler's version check"
};

constexpr Option option_ignore_model_timestamp {
    .name = "--ignore-model-timestamp",
    .short_name = {},
    .help = "Continue even if the model is newer than the mutants"
};

constexpr std::array analyse_parameters {
    option_help,
    option_color,
    option_operator,
    option_include,
    option_json
};

constexpr std::array applyall_parameters {
    option_directory,
    option_help,
    option_color,
    option_operator,
    option_json,
    option_include
};

constexpr std::array run_parameters {
    option_directory,
    option_compiler_path,
    option_help,
    option_in_memory,
    option_color,
    option_operator,
    option_timeout,
    option_data,
    option_jobs,
    option_output,
    option_include,
    option_mutant,
    option_json,
    option_ignore_version_check,
    option_ignore_model_timestamp,
};

constexpr std::array normalise_parameters {
    option_include,
    option_help,
    option_color
};

constexpr std::array clean_parameters {
    option_directory,
    option_help,
    option_color
};

constexpr Command command_applyall {
    .option {
        .name = "applyall",
        .short_name = {},
        .help = "Analyses and then apply all the found mutants to the given MiniZinc model" },
    .operation = applyall,
    .options = applyall_parameters
};

constexpr Command command_analyse {
    .option {
        .name = "analyse",
        .short_name = {},
        .help = "Analyses the given MiniZinc model" },
    .operation = analyse,
    .options = analyse_parameters
};

constexpr Command command_hidden_analyze {
    .option = {
        .name = "analyze",
        .short_name = command_analyse.option.short_name,
        .help = command_analyse.option.help },
    .operation = command_analyse.operation,
    .options = analyse_parameters,
    .is_hidden = true
};

constexpr Command command_run {
    .option {
        .name = "run",
        .short_name = {},
        .help = "Runs all the mutants" },
    .operation = run,
    .options = run_parameters
};

constexpr Command command_normalise {
    .option {
        .name = "normalise",
        .short_name = {},
        .help = "Prints a normalised version of the given model" },
    .operation = normalise,
    .options = normalise_parameters
};

constexpr Command command_hidden_normalize {
    .option {
        .name = "normalize",
        .short_name = command_normalise.option.short_name,
        .help = command_normalise.option.help },
    .operation = normalise,
    .options = normalise_parameters,
    .is_hidden = true
};

constexpr Command command_clean {
    .option {
        .name = "clean",
        .short_name = {},
        .help = "Cleans the working directory for a model" },
    .operation = clean,
    .options = clean_parameters
};

constexpr Command command_hidden_clear {
    .option = {
        .name = "clear",
        .short_name = command_clean.option.short_name,
        .help = command_clean.option.help },
    .operation = command_clean.operation,
    .options = clean_parameters,
    .is_hidden = true
};

constexpr Command command_help {
    .option {
        .name = "help",
        .short_name = {},
        .help = "Print this message or the help of the given subcommand" },
    .operation = help_subcommand,
    .options = {}
};

constexpr Command command_help_option {
    .option = option_help,
    .operation = nullptr,
    .options = {}
};

constexpr Command command_version {
    .option {
        .name = "--version",
        .short_name = "-v",
        .help = "Prints the version" },
    .operation = nullptr,
    .options = {}
};

constexpr Command command_color_option {
    .option = option_color,
    .operation = nullptr,
    .options = {}
};

constexpr std::array commands {
    command_applyall,
    command_analyse,
    command_hidden_analyze,
    command_run,
    command_normalise,
    command_hidden_normalize,
    command_clean,
    command_hidden_clear,
    command_help,
    command_help_option,
    command_version,
    command_color_option
};

nlohmann::json get_statistics_json(const MuMiniZinc::EntryResult& entries)
{
    if (entries.mutants().empty())
        return nlohmann::json::object();

    const auto names_view = entries.mutants()
        | std::views::transform([](const auto& mutant)
            { return mutant.name; });

    const auto stats_view = entries.statistics()
        | std::views::enumerate
        | std::views::transform([](const auto& item)
            {
                          auto [index, stats_pair] = item;
                          return nlohmann::json{
                              {"name", MuMiniZinc::available_operators[static_cast<std::size_t>(index)].first},
                              {"amount", stats_pair.first},
                              {"occurences", stats_pair.second}
                          }; });

    return nlohmann::json {
        { "detected_mutants", names_view | std::ranges::to<std::vector>() },
        { "operator_statistics", stats_view | std::ranges::to<std::vector>() }
    };
};

void print_statistics(const MuMiniZinc::EntryResult& entries)
{
    if (entries.mutants().empty())
    {
        std::println("Could not detect any mutants");
        return;
    }

    std::println("{:s}{:s}Detected mutants{:s}:", logging::code(logging::Style::Bold), logging::code(logging::Style::Underline), logging::code(logging::Style::Reset));

    for (const auto& entry : entries.mutants())
        std::println("  {:s}", entry.name);

    std::print("{0:s}Total{1:s}: {2:s}{3:d}{1:s} mutants.\n\n", logging::code(logging::Style::Bold), logging::code(logging::Style::Reset), logging::code(logging::Color::Blue), entries.mutants().size());

    std::println("{:s}{:s}Operator statistics{:s}:", logging::code(logging::Style::Bold), logging::code(logging::Style::Underline), logging::code(logging::Style::Reset));
    for (auto [n, stats] : entries.statistics() | std::views::enumerate)
        std::println("- {2:s}\n    - Amount:     {0:s}{3:d}{1:s}\n    - Occurences: {0:s}{4:d}{1:s}", logging::code(logging::Color::Blue), logging::code(logging::Style::Reset), MuMiniZinc::available_operators[static_cast<std::size_t>(n)].first, stats.first, stats.second);
}

template<typename Exception>
void throw_operator_option_error(std::string_view message)
{
    auto error_string = std::format("{:s}\n\n{:s}{:s}Available operators{:s}:", message, logging::code(logging::Style::Bold), logging::code(logging::Style::Underline), logging::code(logging::Style::Reset));

    constexpr auto largest_operator = std::ranges::max_element(MuMiniZinc::available_operators,
        {}, [](const auto& element)
        { return element.first; })
                                          ->first.length();

    for (const auto& [name, help] : MuMiniZinc::available_operators)
        error_string += std::format("\n  {:<{}}  {}", name, largest_operator + 2, help);

    throw Exception { error_string };
}

int print_help()
{
    static constexpr auto largest_command = std::ranges::max_element(commands,
        {}, [](const Command& command)
        { return command.is_option() || command.is_hidden ? std::string_view::size_type {} : command.option.name.length(); })
                                                ->option.name.length();

    static constexpr auto largest_option = std::ranges::max_element(commands,
        {}, [](const Command& command)
        { return !command.is_option() ? std::string_view::size_type {} : command.option.name.length(); })
                                               ->option.name.length();

    std::println("{:s} is a mutation test tool for MiniZinc models.", MuMiniZinc::config::project_fancy_name);

    std::println("\n{0:}{1:}Usage{2:}: ./{3:s} [COMMAND]\n\n{0:}{1:}Commands{2:}:", logging::code(logging::Style::Bold), logging::code(logging::Style::Underline), logging::code(logging::Style::Reset), MuMiniZinc::config::executable_name);

    for (const auto& command : commands | std::views::filter([](const auto& command)
                                   { return !command.is_option() && !command.is_hidden; }))
        std::println("  {:<{}}  {}", command.option.name, largest_command, command.option.help);

    std::println("\n{}{}Options{}:", logging::code(logging::Style::Bold), logging::code(logging::Style::Underline), logging::code(logging::Style::Reset));
    for (const auto& option : commands | std::views::filter([](const auto& option)
                                  { return option.is_option() && !option.is_hidden; }))
        std::println("  {}, {:<{}}  {}", option.option.short_name, option.option.name, largest_option, option.option.help);

    return EXIT_SUCCESS;
}

int help_subcommand(const Command& command)
{
    std::println("{}", command.option.help);

    std::println("\n{:s}{:s}Usage{:s}: ./{:s} {:s} <MODEL> <ARGUMENTS>", logging::code(logging::Style::Bold), logging::code(logging::Style::Underline), logging::code(logging::Style::Reset), MuMiniZinc::config::executable_name, command.option.name);

    const auto largest_option = std::ranges::max_element(command.options,
        {}, [](const Option& option)
        { return option.name.length(); });

    if (largest_option != command.options.end())
    {
        const auto lagest_option_length = largest_option->name.length();

        std::println("\n{:s}{:s}Options{:s}:", logging::code(logging::Style::Bold), logging::code(logging::Style::Underline), logging::code(logging::Style::Reset));
        for (const auto& option : command.options)
        {
            if (option.short_name.empty())
                std::println("  {:<{}}  {}", option.name, lagest_option_length, option.help);
            else
                std::println("  {}, {:<{}}  {}", option.short_name, option.name, lagest_option_length - (largest_option->short_name.empty() ? 4 : 0), option.help);
        }
    }

    return EXIT_SUCCESS;
}

int help_subcommand(std::span<const std::string_view> arguments)
{
    // No arguments, or asking for help for ourselves.
    if (arguments.empty() || (arguments.size() == 1 && arguments.front() == command_help.option.name) || (arguments.front() == command_help.option.name && (arguments[1] == option_help || arguments[1] == arguments.front())))
        return print_help();

    if (arguments.size() > 2)
        throw BadArgument { std::format("{:s}: Too many arguments.", arguments.front()) };

    const auto* const command = std::ranges::find_if(commands, [subcommand = arguments.back()](const auto& command)
        { return command.option.name == subcommand; });

    if (command == commands.end())
        throw BadArgument { std::format("{:s}: Unknown command `{:s}{:s}{:s}`.", arguments.front(), logging::code(logging::Color::Blue), arguments.back(), logging::code(logging::Style::Reset)) };

    return help_subcommand(*command);
}

int applyall(std::span<const std::string_view> arguments)
{
    std::string_view model_path;
    std::string_view output_directory;
    std::string_view include_path;
    std::vector<ascii_ci_string_view> allowed_operators;
    bool is_json { false };

    for (std::size_t i { 1 }; i < arguments.size(); ++i)
    {
        if (arguments[i] == option_json)
            is_json = true;
        else if (arguments[i] == option_include)
        {
            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_include.name) };

            include_path = arguments[i + 1];

            ++i;
        }
        else if (arguments[i] == option_operator)
        {
            if (i + 1 >= arguments.size())
                throw_operator_option_error<BadArgument>(std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_operator.name));

            for (const auto op : std::views::split(arguments[i + 1], separator_arguments))
                allowed_operators.emplace_back(op);

            ++i;
        }
        else if (arguments[i] == option_directory)
        {
            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_directory.name) };

            output_directory = arguments[i + 1];
            ++i;
        }
        else if (arguments[i] == option_help)
            return help_subcommand(arguments.subspan(0, 1));
        else if (!model_path.empty())
            throw BadArgument { std::format("{:s}: Unknown parameter `{:s}{:s}{:s}`.", arguments.front(), logging::code(logging::Color::Blue), arguments[i], logging::code(logging::Style::Reset)) };
        else
            model_path = arguments[i];
    }

    if (model_path.empty())
        throw BadArgument { std::format("{:s}: Missing model path.", arguments.front()) };

    try
    {
        const std::filesystem::path model_path_str { model_path };

        const MuMiniZinc::find_mutants_args parameters {
            .model = model_path_str,
            .allowed_operators = allowed_operators,
            .include_path = include_path.empty() ? std::string {} : std::filesystem::canonical(include_path).string(),
            .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
        };

        const auto entries { MuMiniZinc::find_mutants(parameters) };

        if (entries.mutants().empty())
        {
            if (is_json)
                std::println("{}", get_statistics_json(entries).dump());
            else
                print_statistics(entries);
        }
        else
        {
            const auto calculated_output_directory = output_directory.empty() ? MuMiniZinc::get_path_from_model_path(model_path) : std::filesystem::path { output_directory };
            MuMiniZinc::dump_mutants(entries, calculated_output_directory);

            if (is_json)
            {
                auto json = get_statistics_json(entries);

                json.emplace("saved_mutants", entries.mutants().size());
                json.emplace("saved_mutants_directory", calculated_output_directory.native());

                std::println("{:s}", json.dump());
            }
            else
            {
                print_statistics(entries);

                std::println("\nSaved {0:s}{2:d}{1:s} mutants to `{0:s}{3:s}{1:s}`.", logging::code(logging::Color::Blue), logging::code(logging::Style::Reset), entries.mutants().size(), calculated_output_directory.native());
            }
        }
    }

    catch (const MuMiniZinc::UnknownOperator& unknown_operator)
    {
        throw_operator_option_error<MuMiniZinc::UnknownOperator>(std::format("{:s}: {:s}", arguments.front(), unknown_operator.what()));
    }

    return EXIT_SUCCESS;
}

int analyse(std::span<const std::string_view> arguments)
{
    std::string_view model_path;
    std::string_view include_path;
    std::vector<ascii_ci_string_view> allowed_operators;
    bool is_json { false };

    for (std::size_t i { 1 }; i < arguments.size(); ++i)
    {
        if (arguments[i] == option_json)
            is_json = true;
        else if (arguments[i] == option_include)
        {
            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_include.name) };

            include_path = arguments[i + 1];

            ++i;
        }
        else if (arguments[i] == option_operator)
        {
            if (i + 1 >= arguments.size())
                throw_operator_option_error<BadArgument>(std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_operator.name));

            for (const auto op : std::views::split(arguments[i + 1], separator_arguments))
                allowed_operators.emplace_back(op);

            ++i;
        }
        else if (arguments[i] == option_help)
            return help_subcommand(arguments.subspan(0, 1));
        else if (!model_path.empty())
            throw BadArgument { std::format("{:s}: Unknown parameter `{:s}{:s}{:s}`.", arguments.front(), logging::code(logging::Color::Blue), arguments[i], logging::code(logging::Style::Reset)) };
        else
            model_path = arguments[i];
    }

    if (model_path.empty())
        throw BadArgument { std::format("{:s}: Missing model path.", arguments.front()) };

    try
    {
        std::variant<MuMiniZinc::find_mutants_args::ModelDetails, std::reference_wrapper<const std::filesystem::path>> variant;

        std::filesystem::path model_path_str;

        if (model_path == "-"sv)
        {
            std::ostringstream ostringstream;
            ostringstream << std::cin.rdbuf();

            variant.emplace<0>("stdin", std::move(ostringstream).str());
        }
        else
        {
            model_path_str = model_path;
            variant.emplace<1>(model_path_str);
        }

        const MuMiniZinc::find_mutants_args parameters {
            .model = variant,
            .allowed_operators = allowed_operators,
            .include_path = include_path.empty() ? std::string {} : std::filesystem::canonical(include_path).string(),
            .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
        };

        const auto entries { MuMiniZinc::find_mutants(parameters) };

        if (is_json)
            std::println("{:s}", get_statistics_json(entries).dump());
        else
            print_statistics(entries);
    }
    catch (const MuMiniZinc::UnknownOperator& unknown_operator)
    {
        throw_operator_option_error<MuMiniZinc::UnknownOperator>(std::format("{:s}: {:s}", arguments.front(), unknown_operator.what()));
    }

    return EXIT_SUCCESS;
}

int run(std::span<const std::string_view> arguments)
{
    std::string_view model_path;
    std::string_view output_directory;
    std::string_view compiler_path { "minizinc" };
    std::string_view include_path;
    std::span<const std::string_view> remaining_args;
    std::vector<ascii_ci_string_view> allowed_operators;
    bool in_memory { false };
    const char* output { nullptr };
    std::uint64_t n_jobs { default_n_jobs };
    std::vector<std::string> data_files;
    std::vector<ascii_ci_string_view> allowed_mutants;
    bool check_compiler_version { true };
    bool check_model_last_modified_time { true };
    bool is_json { false };

    std::uint64_t timeout_seconds { DEFAULT_TIMEOUT_S };
#undef DEFAULT_TIMEOUT_S

    for (std::size_t i { 1 }; i < arguments.size(); ++i)
    {
        if (arguments[i] == option_json)
            is_json = true;
        else if (arguments[i] == option_ignore_model_timestamp)
        {
            if (in_memory)
                throw BadArgument { std::format("{:s}: {:s}: Argument not compatible with `{:s}{:s}{:s}`.", arguments.front(), option_ignore_model_timestamp.name, logging::code(logging::Color::Blue), option_in_memory.name, logging::code(logging::Style::Reset)) };

            check_model_last_modified_time = false;
        }
        else if (arguments[i] == option_ignore_version_check)
            check_compiler_version = false;
        else if (arguments[i] == option_mutant)
        {
            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_mutant.name) };

            for (const auto mutant : std::views::split(arguments[i + 1], separator_arguments))
                allowed_mutants.emplace_back(mutant);

            ++i;
        }
        else if (arguments[i] == option_include)
        {
            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_include.name) };

            include_path = arguments[i + 1];

            ++i;
        }
        else if (arguments[i] == option_output)
        {
            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_output.name) };

            // This is safe because we know this comes from the argv array, whose strings are always null-terminated.
            output = arguments[i + 1].data();

            ++i;
        }
        else if (arguments[i] == option_jobs)
        {
            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_jobs.name) };

            const auto parameter { arguments[i + 1] };
            const auto [_, ec] = std::from_chars(parameter.data(), parameter.data() + parameter.size(), n_jobs);

            if (ec == std::errc::invalid_argument)
                throw BadArgument { std::format("{:s}: {:s}: Invalid number.", arguments.front(), option_jobs.name) };

            if (ec == std::errc::result_out_of_range)
                throw BadArgument { std::format("{:s}: {:s}: The specified number is too big.", arguments.front(), option_jobs.name) };

            ++i;
        }
        else if (arguments[i] == option_data)
        {
            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_data.name) };

            for (const auto given_path : std::views::split(arguments[i + 1], separator_arguments))
            {
                const std::filesystem::path path { given_path.begin(), given_path.end() };

                if (!std::filesystem::is_directory(path))
                {
                    data_files.emplace_back(path.string());
                    continue;
                }

                for (const auto& element : std::filesystem::directory_iterator(path))
                {
                    if (!element.is_regular_file() && !element.is_symlink())
                        throw BadArgument { std::format("{:s}: {:s}: Found an invalid file or a folder inside the folder \"{:s}{:s}{:s}\".", arguments.front(), option_data.name, logging::code(logging::Color::Blue), given_path, logging::code(logging::Style::Reset)) };

                    data_files.emplace_back(element.path().string());
                }
            }

            ++i;
        }
        else if (arguments[i] == option_timeout)
        {
            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_timeout.name) };

            const auto parameter { arguments[i + 1] };
            const auto [_, ec] = std::from_chars(parameter.data(), parameter.data() + parameter.size(), timeout_seconds);

            if (ec == std::errc::invalid_argument)
                throw BadArgument { std::format("{:s}: {:s}: Invalid number.", arguments.front(), option_timeout.name) };

            if (ec == std::errc::result_out_of_range)
                throw BadArgument { std::format("{:s}: {:s}: The specified number is too big.", arguments.front(), option_timeout.name) };

            ++i;
        }
        else if (arguments[i] == option_operator)
        {
            if (i + 1 >= arguments.size())
                throw_operator_option_error<BadArgument>(std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_operator.name));

            for (const auto op : std::views::split(arguments[i + 1], separator_arguments))
                allowed_operators.emplace_back(op);

            ++i;
        }
        else if (arguments[i] == option_directory)
        {
            if (in_memory)
                throw BadArgument { std::format("{:s}: {:s}: Argument not compatible with `{:s}{:s}{:s}`.", arguments.front(), option_directory.name, logging::code(logging::Color::Blue), option_in_memory.name, logging::code(logging::Style::Reset)) };

            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_directory.name) };

            output_directory = arguments[i + 1];
            ++i;
        }
        else if (arguments[i] == option_help)
            return help_subcommand(command_run);
        else if (arguments[i] == option_compiler_path)
        {
            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_compiler_path.name) };

            compiler_path = arguments[i + 1];
            ++i;
        }
        else if (arguments[i] == option_in_memory)
        {
            if (!output_directory.empty())
                throw BadArgument { std::format("{:s}: {:s}: Argument not compatible with `{:s}{:s}{:s}`.", arguments.front(), option_in_memory.name, logging::code(logging::Color::Blue), option_directory.name, logging::code(logging::Style::Reset)) };

            if (!check_model_last_modified_time)
                throw BadArgument { std::format("{:s}: {:s}: Argument not compatible with `{:s}{:s}{:s}`.", arguments.front(), option_in_memory.name, logging::code(logging::Color::Blue), option_ignore_model_timestamp.name, logging::code(logging::Style::Reset)) };

            in_memory = true;
        }
        else if (arguments[i] == end_of_options_token)
        {
            remaining_args = arguments.subspan(i + 1);

            break;
        }
        else if (!model_path.empty())
            throw BadArgument { std::format("{:s}: Unknown parameter `{:s}{:s}{:s}`.\n\nIf you want to pass arguments to the compiler, put `--` before them.", arguments.front(), logging::code(logging::Color::Blue), arguments[i], logging::code(logging::Style::Reset)) };
        else
            model_path = arguments[i];
    }

    if (model_path.empty())
        throw BadArgument { std::format("{:s}: Missing model path.", arguments.front()) };

    const std::filesystem::path executable_from_user { compiler_path };

    const auto executable = std::filesystem::exists(executable_from_user) ? executable_from_user : boost::process::environment::find_executable(executable_from_user);

    if (executable.empty())
        throw BadArgument { std::format("{:s}: Could not find the executable `{:s}{:s}{:s}`. Please add it to $PATH or provide its path using `{:s}{:s}{:s}.`", arguments.front(), logging::code(logging::Color::Blue), executable_from_user.native(), logging::code(logging::Style::Reset), logging::code(logging::Color::Blue), option_compiler_path.name, logging::code(logging::Style::Reset)) };

    std::optional<std::ofstream> output_file;

    if (output != nullptr)
    {
        output_file = std::ofstream { output };

        if (!output_file->good())
            throw BadArgument { std::format("{:s}: Could not open the output file `{:s}{:s}{:s}`.", arguments.front(), logging::code(logging::Color::Blue), output, logging::code(logging::Style::Reset)) };
    }

    const std::filesystem::path model_path_str { model_path };

    MuMiniZinc::EntryResult entries;

    try
    {
        if (in_memory)
        {
            std::variant<MuMiniZinc::find_mutants_args::ModelDetails, std::reference_wrapper<const std::filesystem::path>> variant;

            std::filesystem::path model_path_str;

            if (model_path == "-"sv)
            {
                std::ostringstream ostringstream;
                ostringstream << std::cin.rdbuf();

                variant.emplace<0>("stdin", std::move(ostringstream).str());
            }
            else
            {
                model_path_str = model_path;
                variant.emplace<1>(model_path_str);
            }

            const MuMiniZinc::find_mutants_args parameters {
                .model = variant,
                .allowed_operators { allowed_operators },
                .include_path { include_path },
                .run_type = MuMiniZinc::find_mutants_args::RunType::FullRun
            };

            entries = MuMiniZinc::find_mutants(parameters);
        }
        else
        {
            const auto calculated_output_directory = output_directory.empty() ? MuMiniZinc::get_path_from_model_path(model_path) : std::filesystem::path { output_directory };

            const MuMiniZinc::retrieve_mutants_args parameters {
                .model_path = model_path_str,
                .directory_path = calculated_output_directory,
                .allowed_operators = allowed_operators,
                .allowed_mutants = allowed_mutants,
                .check_model_last_modified_time = check_model_last_modified_time
            };

            entries = MuMiniZinc::retrieve_mutants(parameters);
        }
    }
    catch (const MuMiniZinc::UnknownOperator& unknown_operator)
    {
        throw_operator_option_error<MuMiniZinc::UnknownOperator>(std::format("{:s}: {:s}", arguments.front(), unknown_operator.what()));
    }

    if (entries.mutants().empty())
        throw std::runtime_error { std::format("{:s}: Could not find any mutants to run.", arguments.front()) };

    const MuMiniZinc::run_mutants_args parameters {
        .entry_result = entries,
        .compiler_path = executable,
        .compiler_arguments = remaining_args,
        .allowed_mutants = allowed_mutants,
        .data_files = data_files,
        .timeout { timeout_seconds },
        .n_jobs = n_jobs,
        .check_compiler_version = check_compiler_version,
        .output_log = is_json ? logging::output {} : logging::output { std::cout }
    };

    std::size_t n_invalid {};
    std::size_t n_alive {};
    std::size_t n_dead {};

    const std::ostreambuf_iterator<char> output_stream { output_file.has_value() ? *output_file : std::cout };

    try
    {
        MuMiniZinc::run_mutants(parameters);

        if (!is_json)
            std::print("\n\n");
    }
    catch (const MuMiniZinc::BadVersion& bad_version)
    {
        throw MuMiniZinc::BadVersion { std::format("{:s}\n\nTo disable the compiler version check, use the option `{:s}{:s}{:s}`. The expected version number is {:s}.", bad_version.what(), logging::code(logging::Color::Blue), option_ignore_version_check.name, logging::code(logging::Style::Reset), MuMiniZinc::get_version()) };
    }
    catch (const MuMiniZinc::OutdatedMutant& outdated_mutant)
    {
        throw MuMiniZinc::OutdatedMutant { std::format("{:s}\n\nTo disable the outdated mutant check, use the option `{:s}{:s}{:s}`.", outdated_mutant.what(), logging::code(logging::Color::Blue), option_ignore_model_timestamp.name, logging::code(logging::Style::Reset)) };
    }

    auto entries_array = nlohmann::json::array();

    for (const auto& entry : entries.mutants())
    {
        if (entry.results.empty())
            continue;

        if (!is_json || (is_json && output_file.has_value()))
            std::format_to(output_stream, "{:<{}}   ", entry.name, entries.model_name().size() + 10);

        auto status = MuMiniZinc::Entry::Status::Dead;

        auto json_results = nlohmann::json::array();
        for (const auto value : entry.results)
        {
            const auto value_underlying = std::to_underlying(value);

            if (!is_json || (is_json && output_file.has_value()))
                std::format_to(output_stream, "{:d} ", value_underlying);

            if (is_json)
                json_results.emplace_back(value_underlying);

            if (status == MuMiniZinc::Entry::Status::Dead)
                status = value;
        }

        if (is_json)
            entries_array.emplace_back(nlohmann::json::object({ { "mutant", entry.name },
                { "results", std::move(json_results) } }));

        switch (status)
        {
        case MuMiniZinc::Entry::Status::Alive:
            ++n_alive;
            break;
        case MuMiniZinc::Entry::Status::Dead:
            ++n_dead;
            break;
        case MuMiniZinc::Entry::Status::Invalid:
            ++n_invalid;
            break;
        }

        if (!is_json || (is_json && output_file.has_value()))
            std::format_to(output_stream, "\n");
    }

    if (!is_json && !output_file.has_value())
        std::println();

    if (is_json)
    {
        const nlohmann::json json {
            { "results", std::move(entries_array) },
            { "summary", { { "invalid", n_invalid }, { "alive", n_alive }, { "dead", n_dead } } }
        };

        std::println("{}", json.dump());
    }
    else
        std::println("{2:s}{3:s}Summary:{0:s}\n  Invalid:  {1:s}{4:d}{0:s}\n  Alive:    {1:s}{5:d}{0:s}\n  Dead:     {1:s}{6:d}{0:s}", logging::code(logging::Style::Reset), logging::code(logging::Color::Blue), logging::code(logging::Style::Bold), logging::code(logging::Style::Underline), n_invalid, n_alive, n_dead);

    return EXIT_SUCCESS;
}

int normalise(std::span<const std::string_view> arguments)
{
    std::string_view model_path;
    std::string_view include_path;

    for (std::size_t i { 1 }; i < arguments.size(); ++i)
    {
        if (arguments[i] == option_include)
        {
            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_include.name) };

            include_path = arguments[i + 1];

            ++i;
        }
        else if (arguments[i] == option_help)
            return help_subcommand(arguments.subspan(0, 1));
        else if (!model_path.empty())
            throw BadArgument { std::format("{:s}: Unknown parameter `{:s}{:s}{:s}`.", arguments.front(), logging::code(logging::Color::Blue), arguments[i], logging::code(logging::Style::Reset)) };
        else
            model_path = arguments[i];
    }

    if (model_path.empty())
        throw BadArgument { std::format("{:s}: Missing model path.", arguments.front()) };

    std::variant<MuMiniZinc::find_mutants_args::ModelDetails, std::reference_wrapper<const std::filesystem::path>> variant;

    std::filesystem::path model_path_str;

    if (model_path == "-"sv)
    {
        std::ostringstream ostringstream;
        ostringstream << std::cin.rdbuf();

        variant.emplace<0>("stdin", std::move(ostringstream).str());
    }
    else
    {
        model_path_str = model_path;
        variant.emplace<1>(model_path_str);
    }

    const MuMiniZinc::find_mutants_args parameters {
        .model = variant,
        .allowed_operators = {},
        .include_path = include_path.empty() ? std::string {} : std::filesystem::canonical(include_path).string(),
        .run_type = MuMiniZinc::find_mutants_args::RunType::NoDetection
    };

    const auto result { MuMiniZinc::find_mutants(parameters) };

    std::print("{:s}", result.normalized_model());

    return EXIT_SUCCESS;
}

int clean(std::span<const std::string_view> arguments)
{
    std::string_view model_path {};
    std::string_view output_directory {};

    for (std::size_t i { 1 }; i < arguments.size(); ++i)
    {
        if (arguments[i] == option_directory)
        {
            if (i + 1 >= arguments.size())
                throw BadArgument { std::format("{:s}: {:s}: Missing parameter.", arguments.front(), option_directory.name) };

            output_directory = arguments[i + 1];
            ++i;
        }
        else if (arguments[i] == option_help)
            return help_subcommand(arguments.subspan(0, 1));
        else if (!model_path.empty())
            throw BadArgument { std::format("{:s}: Unknown parameter `{:s}{:s}{:s}`.", arguments.front(), logging::code(logging::Color::Blue), arguments[i], logging::code(logging::Style::Reset)) };
        else
            model_path = arguments[i];
    }

    if (model_path.empty())
        throw BadArgument { std::format("{:s}: Missing model path.", arguments.front()) };

    const auto calculated_output_directory = output_directory.empty() ? MuMiniZinc::get_path_from_model_path(model_path) : std::filesystem::path { output_directory };

    MuMiniZinc::clear_mutant_output_folder(model_path, calculated_output_directory);

    return EXIT_SUCCESS;
}

int print_version()
{
    std::println("{:s} {:s}\nBuilt with MiniZinc {:s}", MuMiniZinc::config::project_fancy_name, MuMiniZinc::config::project_version, MuMiniZinc::get_version());

    if constexpr (MuMiniZinc::config::is_debug_build)
        std::println("\nDebug build.");

    return EXIT_SUCCESS;
}
}

int parse_arguments(std::span<const char* const> argv)
{
    if (argv.size() < 2)
        return print_help();

    std::vector<std::string_view> arguments;

    // Identify global toggles and remove them from the argument list.
    for (std::size_t i { 1 }; i < argv.size(); ++i)
    {
        const std::string_view argument { argv[i] };

        if (argument == option_help && arguments.empty())
            return print_help();

        if (argument == command_version.option)
            return print_version();

        if (argument == option_color)
        {
            if (i + 1 >= argv.size())
                throw BadArgument { std::format("{:s}: Missing parameter.", option_color.name) };

            const std::string_view value { argv[i + 1] };

            bool should_have_color = false;

            static constexpr auto value_true { "true"sv };
            static constexpr auto value_false { "false"sv };

            if (value == value_true)
                should_have_color = true;
            else if (value == value_false)
                should_have_color = false;
            else
                throw BadArgument { std::format(R"({:s}: Unknown value `{:s}{:s}{:s}`. Valid values are "{:s}" and "{:s}".)", option_color.name, logging::code(logging::Color::Blue), value, logging::code(logging::Style::Reset), value_true, value_false) };

            logging::have_color_stdout = logging::have_color_stderr = should_have_color;

            ++i;
        }
        else if (argument == end_of_options_token)
        {
            // We won't find any arguments after the delimiter.
            for (std::size_t j { i }; j < argv.size(); ++j)
                arguments.emplace_back(argv[j]);

            break;
        }
        else
            arguments.emplace_back(argument);
    }

    const std::span arguments_span { arguments };

    for (std::size_t i {}; i < arguments_span.size(); ++i)
    {
        for (const auto& command : commands)
            if (arguments[i] == command.option)
                // The command's options will be handled by itself.
                return command.operation(arguments_span.subspan(i));

        // If we have reached this point, we have found an argument that does not match
        // our commands or our global arguments.
        throw BadArgument { std::format("Unknown command or option `{:s}{:s}{:s}`.", logging::code(logging::Color::Blue), arguments[i], logging::code(logging::Style::Reset)) };
    }

    // If we have reached this point, we have found no commands to execute.
    return print_help();
}