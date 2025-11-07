#include <mutation.hpp>

#include <algorithm>    // std::ranges::contains, std::ranges::find_if
#include <array>        // std::array
#include <cstddef>      // std::size_t
#include <cstdint>      // std::uint64_t
#include <filesystem>   // std::filesystem::absolute, std::filesystem::create_directory, std::filesystem::directory_iterator, std::filesystem::is_directory, std::filesystem::is_regular_file, std::filesystem::path, std::filesystem::remove_all
#include <format>       // std::format
#include <fstream>      // std::ifstream
#include <functional>   // std::reference_wrapper
#include <iostream>     // std::cerr
#include <iterator>     // std::distance
#include <span>         // std::span
#include <sstream>      // std::ostringstream
#include <stdexcept>    // std::runtime_error
#include <string>       // std::string
#include <string_view>  // std::string_view
#include <system_error> // std::error_code
#include <type_traits>  // std::decay_t
#include <utility>      // std::move, std::pair
#include <variant>      // std::visit
#include <vector>       // std::vector

#include <minizinc/ast.hh>           // MiniZinc::BinOp, MiniZinc::ConstraintI, MiniZinc::EVisitor, MiniZinc::Expression, MiniZinc::OutputI, MiniZinc::SolveI
#include <minizinc/astiterator.hh>   // MiniZinc::top_down
#include <minizinc/aststring.hh>     // MiniZinc::ASTString
#include <minizinc/file_utils.hh>    // MiniZinc::FileUtils::share_directory
#include <minizinc/model.hh>         // MiniZinc::Env
#include <minizinc/parser.hh>        // MiniZinc::parse
#include <minizinc/prettyprinter.hh> // MiniZinc::Printer

#include <build/config.hpp>            // MuMiniZinc::config::is_debug_build
#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <executor.hpp>                // MuMiniZinc::execute_mutants, MuMiniZinc::execution_args
#include <logging.hpp>                 // logd, logging::code, logging::Color, logging::Style
#include <operators.hpp>               // MuMiniZinc::available_operators

namespace
{

using namespace std::string_view_literals;

constexpr auto EXTENSION { ".mzn"sv };
constexpr auto WIDTH_PRINTER { 80 };
constexpr auto SEPARATOR { '-' };
constexpr auto enum_keyword { "enum "sv };

constexpr bool is_quoted(std::string_view view) noexcept
{
    std::uint64_t quotes {};

    for (std::size_t i {}; i < view.size(); ++i)
    {
        if (view[i] == '"' && (i == 0 || view[i - 1] != '\\'))
            ++quotes;
    }

    return quotes % 2 == 1;
}

}

namespace
{

std::string get_stem_if_valid(const std::string_view model_stem, const std::filesystem::directory_entry& entry) noexcept
{
    if (!entry.is_regular_file() || entry.path().extension() != EXTENSION)
        return {};

    auto str = entry.path().stem().string();

    if (str == model_stem || (str.starts_with(model_stem) && str.size() >= model_stem.size() + 1 && str[model_stem.size()] == SEPARATOR))
        return str;

    return {};
}

void dump_file(const std::filesystem::path& path, std::string_view contents)
{
    if (std::filesystem::exists(path))
        throw MuMiniZinc::IOError { std::format("The path `{:s}{:s}{:s}` already exists.", logging::code(logging::Color::Blue), logging::path_to_utf8(path), logging::code(logging::Style::Reset)) };

    std::ofstream file { path };

    if (!file.is_open())
        throw MuMiniZinc::IOError { std::format(R"(Could not open the mutant file `{:s}{:s}{:s}`.)", logging::code(logging::Color::Blue), logging::path_to_utf8(path), logging::code(logging::Style::Reset)) };

    file << contents;

    if (file.bad())
        throw MuMiniZinc::IOError { std::format(R"(Could not write to the file `{:s}{:s}{:s}`.)", logging::code(logging::Color::Blue), logging::path_to_utf8(path), logging::code(logging::Style::Reset)) };
}

constexpr void fix_enums(const std::span<const std::pair<std::string, std::string>>& detected_enums, std::string& model)
{
    for (const auto& [string_to_find, string_to_replace] : detected_enums)
    {
        std::string::size_type pos {};

        while (pos < model.size())
        {
            pos = model.find(string_to_find, pos);

            if (pos == std::string::npos)
                break;

            if (!is_quoted(std::string_view { model }.substr(0, pos)))
            {
                model.replace(pos, string_to_find.size(), string_to_replace);
                break;
            }

            pos += string_to_find.size();
        }
    }
}

constexpr auto get_model = [](auto&& element) -> std::pair<std::string, std::string>
{
    using T = std::decay_t<decltype(element)>;

    if constexpr (std::is_same_v<T, MuMiniZinc::find_mutants_args::ModelDetails>)
        return std::pair { element.name, element.contents };
    else if constexpr (!std::is_same_v<T, std::reference_wrapper<const std::filesystem::path>>)
        static_assert(false, "Unknown variant type.");
    else
    {
        const auto status = std::filesystem::status(element);

        if (!std::filesystem::exists(status))
            throw MuMiniZinc::IOError { std::format("The path `{:s}{:s}{:s}` does not exist.", logging::code(logging::Color::Blue), logging::path_to_utf8(element.get()), logging::code(logging::Style::Reset)) };

        if (std::filesystem::is_directory(status))
            throw MuMiniZinc::IOError { std::format("The path `{:s}{:s}{:s}` is a directory.", logging::code(logging::Color::Blue), logging::path_to_utf8(element.get()), logging::code(logging::Style::Reset)) };

        if (!element.get().has_stem())
            throw MuMiniZinc::IOError { "Could not determine the filename without extension of the model." };

        const std::ifstream ifstream { element.get() };

        std::stringstream buffer;
        buffer << ifstream.rdbuf();

        if (ifstream.bad())
            throw MuMiniZinc::IOError { std::format(R"(Could not open the file `{:s}{:s}{:s}`.)", logging::code(logging::Color::Blue), logging::path_to_utf8(element.get()), logging::code(logging::Style::Reset)) };

        return std::pair { element.get().stem().string(), std::move(buffer).str() };
    }
};

void throw_if_invalid_operators(std::span<const ascii_ci_string_view> allowed_operators)
{
    for (const auto mutant : allowed_operators)
    {
        bool found = false;

        for (const auto& [name, _] : MuMiniZinc::available_operators)
        {
            if (mutant == name)
            {
                found = true;
                break;
            }
        }

        if (!found)
            throw MuMiniZinc::UnknownOperator { std::format("Unknown operator `{:s}{:s}{:s}`.", logging::code(logging::Color::Blue), mutant, logging::code(logging::Style::Reset)) };
    }
}

}

namespace MuMiniZinc
{

void EntryResult::save_model(const MiniZinc::Model* model, std::string_view operator_name, std::uint64_t location_id, std::uint64_t occurrence_id, std::span<const std::pair<std::string, std::string>> detected_enums)
{
    if (model == nullptr)
        throw std::runtime_error { "There is no model to print." };

    std::ostringstream ostringstream;
    MiniZinc::Printer printer(ostringstream, WIDTH_PRINTER, false);
    printer.print(model);

    auto output = std::move(ostringstream).str();

    fix_enums(detected_enums, output);

    const auto it = std::ranges::find_if(available_operators, [operator_name](const auto& element)
        { return element.first == operator_name; });

    if (it == available_operators.end())
        throw UnknownOperator { "Unknown operator found while trying to save the model." };

    const auto operator_id = static_cast<std::size_t>(std::distance(available_operators.begin(), it));

    m_statistics[operator_id].first++;
    m_statistics[operator_id].second = std::max(m_statistics[operator_id].second, occurrence_id);

    auto mutant = std::format("{:s}{:c}{:s}{:c}{:d}{:c}{:d}", m_model_name, SEPARATOR, operator_name, SEPARATOR, location_id, SEPARATOR, occurrence_id);
    m_mutants.emplace_back(std::move(mutant), std::move(output));
}

[[nodiscard]] std::filesystem::path get_path_from_model_path(const std::filesystem::path& model_path)
{
    if ((!model_path.has_relative_path()))
        throw IOError { "Could not automatically determine the path for storing the mutants." };

    if (!model_path.has_stem())
        throw IOError { "Could not determine the filename without extension of the model." };

    return std::filesystem::absolute(model_path.parent_path() / std::format("{:s}-mutants", model_path.stem().string()));
}

[[nodiscard]] EntryResult find_mutants(const find_mutants_args& parameters)
{
    throw_if_invalid_operators(parameters.allowed_operators);

    const auto [model_name, model_contents] = std::visit(get_model, parameters.model);

    if (model_contents.empty())
        throw EmptyFile { "Empty file given. Nothing to do." };

    std::vector<std::string> include_paths;

    if (!parameters.include_path.empty())
    {
        logd("Given include path: {:s}", parameters.include_path);
        include_paths.emplace_back(parameters.include_path);
    }

    const auto share_directory_result = MiniZinc::FileUtils::share_directory();

    if (!share_directory_result.empty())
    {
        logd("Calculated include path: {:s}", share_directory_result);
        include_paths.emplace_back(std::format("{:s}/std/", share_directory_result));
    }

    MiniZinc::Env env;

    const auto* const model = MiniZinc::parse(env, {}, {}, model_contents, model_name, include_paths, {}, false, true, false, config::is_debug_build, std::cerr);

    std::vector<std::pair<std::string, std::string>> detected_enums;

    for (const auto* const item : *model)
    {
        if (const auto* const varDeclI = item->dynamicCast<MiniZinc::VarDeclI>())
        {
            const auto* const expression = varDeclI->e();

            const auto* const type_inst = expression->ti();

            if (type_inst == nullptr || !type_inst->isEnum())
                continue;

            const auto str = expression->id()->v();
            const std::string_view view { str.c_str(), str.size() };

            logd("Detected enum \"{:s}\".", view);

            detected_enums.emplace_back(std::format("set of int: {:s}", view),
                std::format("{:s}{:s}", enum_keyword, view));
        }
    }

    EntryResult entry_result;

    entry_result.m_model_name = model_name;

    std::ostringstream ostringstream;
    MiniZinc::Printer printer(ostringstream, WIDTH_PRINTER, false);
    printer.print(model);

    entry_result.m_model_contents = std::move(ostringstream).str();

    fix_enums(detected_enums, entry_result.m_model_contents);

    if (parameters.run_type == MuMiniZinc::find_mutants_args::RunType::FullRun)
    {
        Mutator mutator { model, parameters.allowed_operators, entry_result, detected_enums };

        for (const auto* const item : *model)
        {
            if (const auto* constraintI = item->dynamicCast<MiniZinc::ConstraintI>())
                MiniZinc::top_down(mutator, constraintI->e());
            else if (const auto* solveI = item->dynamicCast<MiniZinc::SolveI>(); solveI != nullptr && solveI->e() != nullptr)
                MiniZinc::top_down(mutator, solveI->e());
            else if (const auto* outputI = item->dynamicCast<MiniZinc::OutputI>())
                MiniZinc::top_down(mutator, outputI->e());
        }
    }

    return entry_result;
}

[[nodiscard]] EntryResult retrieve_mutants(const retrieve_mutants_args& parameters)
{
    throw_if_invalid_operators(parameters.allowed_operators);

    if (!std::filesystem::is_directory(parameters.directory_path))
        throw IOError { std::format("The directory `{:s}{:s}{:s}` does not exist.", logging::code(logging::Color::Blue), logging::path_to_utf8(parameters.model_path), logging::code(logging::Style::Reset)) };

    if (!parameters.model_path.has_stem())
        throw IOError { "Could not determine the filename without extension of the model. (retrieve)" };

    if (!std::filesystem::exists(parameters.model_path))
        throw IOError { std::format("The path `{:s}{:s}{:s}` does not exist.", logging::code(logging::Color::Blue), logging::path_to_utf8(parameters.model_path), logging::code(logging::Style::Reset)) };

    EntryResult entry_result;
    entry_result.m_model_name = parameters.model_path.stem().generic_string();

    std::error_code last_write_ec {};
    const auto last_write_time_original = parameters.check_model_last_modified_time ? std::filesystem::last_write_time(parameters.model_path, last_write_ec) : std::filesystem::file_time_type::min();

    // Insert all the mutants found in the directory, including the normalized model.
    for (const auto& entry : std::filesystem::directory_iterator { parameters.directory_path })
    {
        auto stem = get_stem_if_valid(entry_result.m_model_name, entry);
        const auto is_normalized_model = stem == entry_result.m_model_name;

        if (!is_normalized_model && stem.empty())
            throw InvalidFile { "One or more elements inside the selected path are not models or mutants from the specified model. Can't run the mutants." };

        if (last_write_time_original > std::filesystem::file_time_type::min() && !last_write_ec)
        {
            const auto last_write_time_mutant { std::filesystem::last_write_time(entry, last_write_ec) };

            if (!last_write_ec && last_write_time_original > last_write_time_mutant)
                throw OutdatedMutant { "The original model is newer than the mutants, so they might be outdated. Please re-analyse the original model." };
        }

        if (!parameters.allowed_operators.empty() && !is_normalized_model)
        {
            ascii_ci_string_view entry_view { stem };

            if (const auto pos = entry_view.find_first_not_of(ascii_ci_string_view { entry_result.m_model_name }); pos != ascii_ci_string_view::npos)
                entry_view = entry_view.substr(pos + 1);

            if (std::ranges::none_of(parameters.allowed_operators, [entry_view](auto op)
                    { return entry_view.contains(op); }))
                continue;
        }

        if (!parameters.allowed_mutants.empty() && !is_normalized_model && !std::ranges::contains(parameters.allowed_mutants, ascii_ci_string_view { stem }))
            continue;

        const std::ifstream ifstream { entry.path() };
        std::stringstream buffer;
        buffer << ifstream.rdbuf();

        if (ifstream.bad())
            throw IOError { std::format(R"(Could not open the file `{:s}`.)", logging::path_to_utf8(entry.path())) };

        auto str = std::move(buffer).str();

        if (str.empty())
            throw EmptyFile { std::format("The file `{:s}{:s}{:s}` is empty.", code(logging::Color::Blue), logging::path_to_utf8(entry.path()), code(logging::Style::Reset)) };

        if (is_normalized_model)
            entry_result.m_model_contents = std::move(str);
        else
            entry_result.m_mutants.emplace_back(std::move(stem), std::move(str));
    }

    return entry_result;
}

void dump_mutants(const EntryResult& entries, const std::filesystem::path& directory)
{
    if (entries.mutants().empty())
        return;

    if (std::filesystem::exists(directory))
    {
        if (!std::filesystem::is_directory(directory))
            throw MuMiniZinc::IOError { std::format("The selected path for storing the mutants, `{:s}{:s}{:s}`, is not a directory.", logging::code(logging::Color::Blue), logging::path_to_utf8(directory), logging::code(logging::Style::Reset)) };
    }
    else
    {
        std::error_code error_code;

        if (!std::filesystem::create_directory(directory, error_code) || error_code)
            throw MuMiniZinc::IOError { std::format("Could not create the directory `{:s}{:s}{:s}`.", logging::code(logging::Color::Blue), logging::path_to_utf8(directory), logging::code(logging::Style::Reset)) };
    }

    if (!std::filesystem::is_empty(directory))
        throw MuMiniZinc::IOError { std::format("The selected path for storing the mutants, `{:s}{:s}{:s}`, is non-empty. Please clean it first to avoid accidental data loss.", logging::code(logging::Color::Blue), logging::path_to_utf8(directory), logging::code(logging::Style::Reset)) };

    for (const auto& mutant : entries.mutants())
    {
        const auto path = (directory / mutant.name).replace_extension(EXTENSION);
        dump_file(path, mutant.contents);
    }

    // Dump the normalized model.
    const auto path = (directory / entries.model_name()).replace_extension(EXTENSION);
    dump_file(path, entries.normalized_model());
}

void run_mutants(const run_mutants_args& parameters)
{
    if (parameters.entry_result.mutants().empty())
        return;

    const execution_args configuration {
        .compiler_path = parameters.compiler_path,
        .compiler_arguments = parameters.compiler_arguments,
        .data_files = parameters.data_files,
        .entries = parameters.entry_result.m_mutants,
        .normalized_model = parameters.entry_result.normalized_model(),
        .timeout = parameters.timeout,
        .n_jobs = parameters.n_jobs,
        .allowed_mutants = parameters.allowed_mutants,
        .check_compiler_version = parameters.check_compiler_version,
        .output_log = parameters.output_log
    };

    execute_mutants(configuration);
}

void clear_mutant_output_folder(const std::filesystem::path& model_path, const std::filesystem::path& output_directory)
{
    if (output_directory.empty())
        throw std::runtime_error { "There is nothing to clear." };

    if (!std::filesystem::is_directory(output_directory))
        throw IOError { std::format("Folder `{:s}` does not exist.", logging::path_to_utf8(output_directory)) };

    const auto model_path_stem = model_path.stem().string();

    for (const auto& entry : std::filesystem::directory_iterator { output_directory })
        if (get_stem_if_valid(model_path_stem, entry).empty())
            throw InvalidFile { "One or more elements inside the selected path are not models or mutants from the specified model. Cannot automatically remove the output folder." };

    std::filesystem::remove_all(output_directory);
}

} // namespace MuMiniZinc