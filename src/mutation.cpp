#include <mutation.hpp>

#include <algorithm>   // std::ranges::contains
#include <array>       // std::array, std::end
#include <chrono>      // std::chrono::seconds
#include <cstdint>     // std::uint64_t
#include <filesystem>  // std::filesystem::absolute, std::filesystem::create_directory, std::filesystem::directory_iterator, std::filesystem::is_directory, std::filesystem::is_regular_file, std::filesystem::path, std::filesystem::remove_all
#include <format>      // std::format
#include <fstream>     // std::fstream
#include <iostream>    // std::cerr, std::cout
#include <optional>    // std::optional
#include <print>       // std::println
#include <span>        // std::span
#include <sstream>     // std::ostringstream
#include <stdexcept>   // std::runtime_error
#include <string>      // std::string
#include <string_view> // std::string_view
#include <utility>     // std::move, std::pair

#include <minizinc/ast.hh>           // MiniZinc::BinOp, MiniZinc::BinOpType, MiniZinc::ConstraintI, MiniZinc::EVisitor, MiniZinc::Expression, MiniZinc::OutputI, MiniZinc::SolveI
#include <minizinc/astiterator.hh>   // MiniZinc::top_down
#include <minizinc/aststring.hh>     // MiniZinc::ASTString
#include <minizinc/model.hh>         // MiniZinc::Env
#include <minizinc/parser.hh>        // MiniZinc::parse
#include <minizinc/prettyprinter.hh> // MiniZinc::Printer

#include <boost/filesystem/path.hpp> // boost::filesystem::path

#include <build/config.hpp> // config::is_debug_build
#include <executor.hpp>     // MutantExecutor
#include <logging.hpp>      // logd

namespace
{

using namespace std::string_view_literals;

constexpr auto EXTENSION { ".mzn"sv };
constexpr auto WIDTH_PRINTER { 80 };
constexpr auto SEPARATOR { '-' };

constexpr std::array relational_operators {
    MiniZinc::BinOpType::BOT_LE,
    MiniZinc::BinOpType::BOT_LQ,
    MiniZinc::BinOpType::BOT_GR,
    MiniZinc::BinOpType::BOT_GQ,
    MiniZinc::BinOpType::BOT_EQ,
    MiniZinc::BinOpType::BOT_NQ,
};

constexpr auto relational_operators_name { "REL"sv };

constexpr std::array arithmetic_operators {
    MiniZinc::BinOpType::BOT_PLUS,
    MiniZinc::BinOpType::BOT_MINUS,
    MiniZinc::BinOpType::BOT_MULT,
    MiniZinc::BinOpType::BOT_DIV,
    MiniZinc::BinOpType::BOT_IDIV,
    MiniZinc::BinOpType::BOT_MOD,
    MiniZinc::BinOpType::BOT_POW,
};

constexpr auto arithmetic_operators_name { "ART"sv };

constexpr std::array unary_operators {
    MiniZinc::UnOpType::UOT_MINUS,
    MiniZinc::UnOpType::UOT_PLUS,
    // MiniZinc::UnOpType::UOT_NOT, (do we want to use this one?)
};

constexpr auto unary_operators_name { "UNA"sv };

const std::array calls {
    MiniZinc::Constants::constants().ids.forall,
    MiniZinc::Constants::constants().ids.exists,
};

constexpr auto call_name { "CALL"sv };

constexpr std::array mutant_help {
    std::pair { relational_operators_name, "Relational operators"sv },
    std::pair { arithmetic_operators_name, "Arithmetic operators"sv },
    std::pair { unary_operators_name, "Unary operators"sv },
    std::pair { call_name, "Function calls"sv }
};

}

/*
 * This actually does not override anything because MiniZinc::EVisitor does not have
 * any virtual members, but we'll follow the convention. This is just static dispatching.
 *
 * The problem with this approach is that MiniZinc::EVisitor's visitors receive a pointer to
 * const, which makes it impossible to actually alter the AST unless we `const_cast` away the
 * const from the pointer. Of course, we know that the object is not actually const, so this is
 * not undefined behavior. And, as we don't actually override anything, we can just have non-
 * const arguments and the static dispatch will make it fail to compile in case we ever receive
 * an actual const object as a parameter for any of the visitors.
 *
 * There is an alternative, which is to "override" the `enter` method, which receives a pointer
 * to a non-cost expression object. Then, we can dynamically determine its type. The problem is
 * that, for whatever reason, the `enter` method is called twice for many operators in the AST,
 * which would yield duplicated mutants, so we can't do that.
 */
class MutationModel::Mutator : public MiniZinc::EVisitor
{
public:
    constexpr explicit Mutator(MutationModel& mutation_model) noexcept;

    void vBinOp(MiniZinc::BinOp* binOp);

    void vUnOp(MiniZinc::UnOp* unOp);

    void vCall(MiniZinc::Call* call);

    [[nodiscard]] constexpr auto generated_mutants() const noexcept { return m_mutation_BinOp_count + m_mutation_UnOp_count + m_mutation_Call_count; }

private:
    MutationModel& m_mutation_model;

    void perform_mutation(MiniZinc::BinOp* op, std::span<const MiniZinc::BinOpType> operators, std::string_view operator_name);
    std::uint64_t m_mutation_BinOp_count {};

    void perform_mutation(MiniZinc::UnOp* op, std::span<const MiniZinc::UnOpType> operators, std::string_view operator_name);
    std::uint64_t m_mutation_UnOp_count {};

    void perform_mutation(MiniZinc::Call* call, std::span<const MiniZinc::ASTString> calls, std::string_view operator_name);
    std::uint64_t m_mutation_Call_count {};
};

constexpr MutationModel::Mutator::Mutator(MutationModel& mutation_model) noexcept :
    m_mutation_model { mutation_model }
{
}

void MutationModel::Mutator::vBinOp(MiniZinc::BinOp* binOp)
{
    logd("vBinOP: Detected operation {}", binOp->opToString().c_str());

    std::span<const MiniZinc::BinOpType> operators;
    std::string_view operator_name;

    if (std::ranges::contains(relational_operators, binOp->op()))
    {
        operator_name = relational_operators_name;
        operators = relational_operators;
    }
    else if (std::ranges::contains(arithmetic_operators, binOp->op()))
    {
        operator_name = arithmetic_operators_name;
        operators = arithmetic_operators;
    }

    if (operators.empty())
        logd("MutationModel::Mutator::vBinOp: Undetected mutation type");
    else if (m_mutation_model.m_allowed_operators.empty() || std::ranges::contains(m_mutation_model.m_allowed_operators, operator_name))
        perform_mutation(binOp, operators, operator_name);
}

void MutationModel::Mutator::vUnOp(MiniZinc::UnOp* unOp)
{
    logd("vUnOp: Detected operation {}", unOp->opToString().c_str());

    if (std::ranges::contains(unary_operators, unOp->op()) && (m_mutation_model.m_allowed_operators.empty() || std::ranges::contains(m_mutation_model.m_allowed_operators, unary_operators_name)))
        perform_mutation(unOp, unary_operators, unary_operators_name);
    else
        logd("MutationModel::Mutator::vUnOp: Undetected mutation type");
}

void MutationModel::Mutator::vCall(MiniZinc::Call* call)
{
    logd("vCall: Detected call to {}", call->id().c_str());

    if (std::ranges::contains(calls, call->id()) && (m_mutation_model.m_allowed_operators.empty() || std::ranges::contains(m_mutation_model.m_allowed_operators, call_name)))
        perform_mutation(call, calls, call_name);
    else
        logd("MutationModel::Mutator::vCall: Unhandled call operation");
}

void MutationModel::Mutator::perform_mutation(MiniZinc::BinOp* op, std::span<const MiniZinc::BinOpType> operators, std::string_view operator_name)
{
    const auto original_operator = op->op();
    const auto& loc = MiniZinc::Expression::loc(op);

    if constexpr (config::is_debug_build)
    {
        [[maybe_unused]] const auto& lhs = MiniZinc::Expression::loc(op->lhs());
        [[maybe_unused]] const auto& rhs = MiniZinc::Expression::loc(op->rhs());

        logd("Mutating {}-{} to {}-{}. LHS: {}-{} to {}-{}. RHS: {}-{} to {}-{}",
            loc.firstLine(), loc.firstColumn(), loc.lastLine(), loc.lastColumn(),
            lhs.firstLine(), lhs.firstColumn(), lhs.lastLine(), lhs.lastColumn(),
            rhs.firstLine(), rhs.firstColumn(), rhs.lastLine(), rhs.lastColumn());
    }

    std::uint64_t occurrence_id {};

    for (auto candidate_operator : operators)
    {
        if (original_operator == candidate_operator)
            continue;

        new (op) MiniZinc::BinOp(loc, op->lhs(), candidate_operator, op->rhs());

        logd("Mutating to {}", op->opToString().c_str());

        m_mutation_model.save_current_model(operator_name, m_mutation_BinOp_count++, occurrence_id++);
    }

    // Go back to the original for the next iteration.
    new (op) MiniZinc::BinOp(loc, op->lhs(), original_operator, op->rhs());
}

void MutationModel::Mutator::perform_mutation(MiniZinc::UnOp* op, std::span<const MiniZinc::UnOpType> operators, std::string_view operator_name)
{
    const auto original_operator = op->op();
    const auto& loc = MiniZinc::Expression::loc(op);

    logd("Mutating {}-{} to {}-{}",
        loc.firstLine(), loc.firstColumn(), loc.lastLine(), loc.lastColumn());

    std::uint64_t occurrence_id {};

    for (auto candidate_operator : operators)
    {
        if (original_operator == candidate_operator)
            continue;

        new (op) MiniZinc::UnOp(loc, candidate_operator, op->e());

        logd("Mutating to {}", op->opToString().c_str());

        m_mutation_model.save_current_model(operator_name, m_mutation_UnOp_count++, occurrence_id++);
    }

    // Go back to the original for the next iteration.
    new (op) MiniZinc::UnOp(loc, original_operator, op->e());
}

void MutationModel::Mutator::perform_mutation(MiniZinc::Call* call, std::span<const MiniZinc::ASTString> calls, std::string_view operator_name)
{
    const auto original_call = call->id();

    if constexpr (config::is_debug_build)
    {
        [[maybe_unused]] const auto& loc = MiniZinc::Expression::loc(call);

        logd("Mutating {}-{} to {}-{}",
            loc.firstLine(), loc.firstColumn(), loc.lastLine(), loc.lastColumn());
    }

    std::uint64_t occurrence_id {};

    for (const auto& candidate_call : calls)
    {
        if (original_call == candidate_call)
            continue;

        logd("Mutating from {} to {}", original_call.c_str(), candidate_call.c_str());

        call->id(candidate_call);

        m_mutation_model.save_current_model(operator_name, m_mutation_Call_count++, occurrence_id++);
    }

    call->id(original_call);
}

MutationModel::MutationModel(const std::filesystem::path& path, std::span<const std::string_view> allowed_operators) :
    m_model_path { std::filesystem::absolute(path) }, m_allowed_operators { allowed_operators }
{
    for (const auto mutant : m_allowed_operators)
    {
        bool found = false;

        for (const auto& [name, _] : MutationModel::get_available_operators())
        {
            if (mutant == name)
            {
                found = true;
                break;
            }
        }

        if (!found)
            throw std::runtime_error { std::format("Unknown mutant `{:s}{:s}{:s}`.", logging::code(logging::Color::Blue), mutant, logging::code(logging::Style::Reset)) };
    }

    if (!std::filesystem::is_regular_file(m_model_path))
        throw std::runtime_error { "Could not open the requested file." };

    if (!m_model_path.has_stem())
        throw std::runtime_error { "Could not determine the filename without extension of the model." };

    m_filename_stem = m_model_path.stem().generic_string();
}

MutationModel::MutationModel(const std::filesystem::path& path, std::string_view output_directory, std::span<const std::string_view> allowed_operators) :
    MutationModel { path, allowed_operators }
{
    // Create the folder that will hold all the mutant code.
    // First, we need to determine the name of the folder.
    // If the given path is /test_file.mzn, then the folder will be /test_file/.
    if ((output_directory.empty() && !m_model_path.has_relative_path()))
        throw std::runtime_error { "Could not automatically determine the path for storing the mutants." };

    m_mutation_folder_path = std::filesystem::absolute(output_directory.empty() ? m_model_path.parent_path() / std::format("{:s}-mutants", m_filename_stem) : output_directory);
}

void MutationModel::clear_output_folder()
{
    if (m_mutation_folder_path.empty())
    {
        if (m_memory.empty())
            throw std::runtime_error { "There is nothing to clear." };

        m_memory.clear();
    }

    if (!std::filesystem::is_directory(m_mutation_folder_path))
        throw std::runtime_error { std::format(R"(Folder "{:s}" does not exist.)", m_mutation_folder_path.native()) };

    for (const auto& entry : std::filesystem::directory_iterator { m_mutation_folder_path })
        if (!get_stem_if_valid(entry))
            throw std::runtime_error { R"(One or more elements inside the selected path are not models or mutants from the specified model. Cannot automatically remove the output folder.)" };

    std::filesystem::remove_all(m_mutation_folder_path);
}

void MutationModel::clear_memory() noexcept
{
    m_memory.clear();
}

bool MutationModel::find_mutants()
{
    MiniZinc::Env env;

    m_model = MiniZinc::parse(env, { m_model_path }, {}, "", "", {}, {}, false, true, false, config::is_debug_build, std::cerr);

    if (!m_mutation_folder_path.empty())
    {
        // If a folder with such name exists, then we're OK as long as it's empty. If it has contents, there might be
        // code from an old analysis.
        // Of course, we'll create the folder if it doesn't already exist.
        if (std::filesystem::is_directory(m_mutation_folder_path))
        {
            if (!std::filesystem::is_empty(m_mutation_folder_path))
                throw std::runtime_error { std::format(R"(The selected path for storing the mutants, `{:s}`, is non-empty. Please run `clean` first or manually remove or empty the folder to avoid accidental data loss.)", m_mutation_folder_path.generic_string()) };
        }
        else
            std::filesystem::create_directory(m_mutation_folder_path);
    }

    // Print the original file passed through the compiler to strip out any comments so we can
    // diff from it cleanly.
    save_current_model({}, 0, 0);

    Mutator mutator { *this };

    for (const auto* item : *m_model)
    {
        if (const auto* constraintI = item->dynamicCast<MiniZinc::ConstraintI>())
            MiniZinc::top_down(mutator, constraintI->e());
        else if (const auto* solveI = item->dynamicCast<MiniZinc::SolveI>(); solveI != nullptr && solveI->e() != nullptr)
            MiniZinc::top_down(mutator, solveI->e());
        else if (const auto* outputI = item->dynamicCast<MiniZinc::OutputI>())
            MiniZinc::top_down(mutator, outputI->e());
    }

    const auto generated_mutants = mutator.generated_mutants();

    if (generated_mutants == 0)
        std::println("Couldn't detect any mutants.");
    else
        std::println("\nGenerated {:s}{:d}{:s} mutants.", logging::code(logging::Color::Blue), generated_mutants, logging::code(logging::Style::Reset));

    return generated_mutants != 0;
}

void MutationModel::save_current_model(std::string_view mutant_name, std::uint64_t mutant_id, std::uint64_t occurrence_id)
{
    if (m_model == nullptr)
        throw std::runtime_error { "There is no model to print." };

    const auto mutant = std::format("{:s}{:c}{:s}{:c}{:d}{:c}{:d}", m_filename_stem, SEPARATOR, mutant_name, SEPARATOR, mutant_id, SEPARATOR, occurrence_id);

    if (!mutant_name.empty())
        std::println("Generating mutant `{:s}{:s}{:s}`", logging::code(logging::Color::Blue), mutant, logging::code(logging::Style::Reset));

    if (m_mutation_folder_path.empty())
    {
        std::ostringstream ostringstream;
        MiniZinc::Printer printer(ostringstream, WIDTH_PRINTER, false);
        printer.print(m_model);

        if (m_memory.empty())
        {
            if (!mutant_name.empty())
                throw std::runtime_error { "Trying to store a mutant when the original source hasn't been stored." };

            m_memory.emplace_back(m_filename_stem, std::move(ostringstream).str());
        }
        else
        {
            if (mutant_name.empty())
                throw std::runtime_error { "Trying to store the original source more than once." };

            m_memory.emplace_back(mutant, std::move(ostringstream).str());
        }
    }
    else
    {
        const auto path = (m_mutation_folder_path / (mutant_name.empty() ? m_filename_stem : mutant)).replace_extension(EXTENSION);

        if (std::filesystem::exists(path))
            throw std::runtime_error { std::format(R"(A mutant with the path "{:s}" already exists. This shouldn't happen.)", path.native()) };

        auto file = std::ofstream(path);

        if (!file.is_open())
            throw std::runtime_error { std::format(R"(Could not open the mutant file "{:s}")", path.native()) };

        MiniZinc::Printer printer(file, WIDTH_PRINTER, false);
        printer.print(m_model);
    }
}

std::span<const MutationModel::Entry> MutationModel::run_mutants(const boost::filesystem::path& compiler_path, std::span<const std::string_view> compiler_arguments, std::span<const std::string_view> data_files, std::chrono::seconds timeout, std::uint64_t n_jobs)
{
    if (m_memory.empty() && !std::filesystem::is_directory(m_mutation_folder_path))
        throw std::runtime_error { std::format(R"(Folder "{:s}" does not exist.)", m_mutation_folder_path.native()) };

    if (m_mutation_folder_path.empty() && m_memory.size() == 1)
        throw std::runtime_error { "Couldn't run any mutants because there aren't any." };

    if (m_memory.empty())
    {
        // Insert the original model first.
        const std::ifstream ifstream { m_model_path };
        std::stringstream buffer;
        buffer << ifstream.rdbuf();

        if (ifstream.bad())
            throw std::runtime_error { std::format(R"(Could not open the file "{:s}".)", m_model_path.native()) };

        m_memory.emplace_back(m_filename_stem, std::move(buffer).str());

        // Insert all the mutants found in the folder, but skip the original model.
        for (const auto& entry : std::filesystem::directory_iterator { m_mutation_folder_path })
        {
            const auto opt = get_stem_if_valid(entry);

            if (!opt)
                throw std::runtime_error { "One or more elements inside the selected path are not models or mutants from the specified model. Can't run the mutants." };

            if (*opt == m_filename_stem)
                continue;

            if (!m_allowed_operators.empty())
            {
                std::string_view entry_view = *opt;

                if (const auto pos = entry_view.find_first_not_of(m_filename_stem); pos != std::string_view::npos)
                    entry_view = entry_view.substr(pos + 1);

                if (std::ranges::none_of(m_allowed_operators, [entry_view](auto op)
                        { return entry_view.contains(op); }))
                    continue;
            }

            const std::ifstream ifstream { entry.path() };
            std::stringstream buffer;
            buffer << ifstream.rdbuf();

            if (ifstream.bad())
                throw std::runtime_error { std::format(R"(Could not open the file "{:s}".)", m_model_path.native()) };

            m_memory.emplace_back(*std::move(opt), std::move(buffer).str());
        }
    }

    execute_mutants(compiler_path, compiler_arguments, data_files, m_memory, timeout, n_jobs);

    return m_memory;
}

[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>> MutationModel::get_available_operators()
{
    return mutant_help;
}

std::optional<std::string> MutationModel::get_stem_if_valid(const std::filesystem::directory_entry& entry) const noexcept
{
    if (!entry.is_regular_file() || entry.path().extension() != EXTENSION)
        return std::nullopt;

    auto str = entry.path().stem().string();

    if (str == m_filename_stem || (str.starts_with(m_filename_stem) && str.size() >= m_filename_stem.size() + 1 && str[m_filename_stem.size()] == SEPARATOR))
        return str;

    return std::nullopt;
}
