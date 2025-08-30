#include <mutation.hpp>

#include <algorithm>   // std::ranges::contains
#include <array>       // std::array, std::end
#include <cstdint>     // std::uint64_t
#include <cstdlib>     // EXIT_SUCCESS
#include <filesystem>  // std::filesystem::absolute, std::filesystem::create_directory, std::filesystem::directory_iterator, std::filesystem::is_directory, std::filesystem::is_regular_file, std::filesystem::path, std::filesystem::remove_all
#include <format>      // std::format
#include <fstream>     // std::fstream
#include <iostream>    // std::cerr, std::cout
#include <print>       // std::println
#include <ranges>      // std::views::drop
#include <span>        // std::span
#include <sstream>     // std::ostringstream
#include <stdexcept>   // std::runtime_error
#include <string>      // std::string
#include <string_view> // std::string_view
#include <utility>     // std::pair
#include <vector>      // std::vector

#include <build/config.hpp> // config::is_debug_build
#include <logging.hpp>      // logd

#include <boost/asio/buffer.hpp>             // boost::asio::buffer, boost::asio::dynamic_buffer
#include <boost/asio/error.hpp>              // boost::asio::error::eof
#include <boost/asio/io_context.hpp>         // boost::asio::io_context
#include <boost/asio/read.hpp>               // boost::asio::read
#include <boost/asio/readable_pipe.hpp>      // boost::asio::readable_pipe
#include <boost/asio/writable_pipe.hpp>      // boost::asio::writable_pipe
#include <boost/asio/write.hpp>              // boost::asio::write
#include <boost/filesystem/path.hpp>         // boost::filesystem::path
#include <boost/process/v2/process.hpp>      // boost::process::process
#include <boost/process/v2/stdio.hpp>        // boost::process::process_stdio
#include <boost/system/error_code.hpp>       // boost::system::error_code
#include <boost/utility/string_view_fwd.hpp> // boost::string_view

#include <minizinc/ast.hh>           // MiniZinc::BinOp, MiniZinc::BinOpType, MiniZinc::ConstraintI, MiniZinc::EVisitor, MiniZinc::Expression, MiniZinc::OutputI, MiniZinc::SolveI
#include <minizinc/astiterator.hh>   // MiniZinc::top_down
#include <minizinc/aststring.hh>     // MiniZinc::ASTString
#include <minizinc/model.hh>         // MiniZinc::Env
#include <minizinc/parser.hh>        // MiniZinc::parse
#include <minizinc/prettyprinter.hh> // MiniZinc::Printer

namespace
{
constexpr std::array relational_operators {
    MiniZinc::BinOpType::BOT_LE,
    MiniZinc::BinOpType::BOT_LQ,
    MiniZinc::BinOpType::BOT_GR,
    MiniZinc::BinOpType::BOT_GQ,
    MiniZinc::BinOpType::BOT_EQ,
    MiniZinc::BinOpType::BOT_NQ,
};

constexpr std::array arithmetic_operators {
    MiniZinc::BinOpType::BOT_PLUS,
    MiniZinc::BinOpType::BOT_MINUS,
    MiniZinc::BinOpType::BOT_MULT,
    MiniZinc::BinOpType::BOT_DIV,
    MiniZinc::BinOpType::BOT_IDIV,
    MiniZinc::BinOpType::BOT_MOD,
    MiniZinc::BinOpType::BOT_POW,
};

constexpr std::array unary_operators {
    MiniZinc::UnOpType::UOT_MINUS,
    MiniZinc::UnOpType::UOT_PLUS,
    // MiniZinc::UnOpType::UOT_NOT, (do we want to use this one?)
};

const std::array calls {
    MiniZinc::Constants::constants().ids.forall,
    MiniZinc::Constants::constants().ids.exists,
};

std::pair<int, std::string> run_program(boost::asio::io_context& ctx, const boost::filesystem::path& compiler_path, std::span<boost::string_view> program_arguments, const std::string& stdin_string = {})
{
    boost::asio::writable_pipe in_pipe { ctx };
    boost::asio::readable_pipe out_pipe { ctx };
    boost::asio::readable_pipe err_pipe { ctx };

    boost::process::process proc(ctx,
        compiler_path,
        program_arguments, boost::process::process_stdio { .in = in_pipe, .out = out_pipe, .err = err_pipe });

    boost::system::error_code error_code;

    if (!stdin_string.empty())
    {
        boost::asio::write(in_pipe, boost::asio::buffer(stdin_string), error_code);
        in_pipe.close();
    }

    std::string output_out;
    std::string output_err;

    boost::asio::read(out_pipe, boost::asio::dynamic_buffer(output_out), error_code);
    boost::asio::read(err_pipe, boost::asio::dynamic_buffer(output_err), error_code);

    if (error_code && !(error_code == boost::asio::error::eof))
        throw std::runtime_error { "run: Cannot grab the output of the compiler." };

    const auto status = proc.wait();

    return { status, status == EXIT_SUCCESS ? output_out : output_err };
}
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
    explicit Mutator(MutationModel& mutation_model);

    void vBinOp(MiniZinc::BinOp* binOp);

    void vUnOp(MiniZinc::UnOp* unOp);

    void vCall(MiniZinc::Call* call);

    [[nodiscard]] auto generated_mutants() const noexcept { return mutation_BinOp_count + mutation_UnOp_count + mutation_Call_count; }

private:
    MutationModel& m_mutation_model;

    void perform_mutation(MiniZinc::BinOp* op, std::span<const MiniZinc::BinOpType> operators);
    std::uint64_t mutation_BinOp_count {};

    void perform_mutation(MiniZinc::UnOp* op, std::span<const MiniZinc::UnOpType> operators);
    std::uint64_t mutation_UnOp_count {};

    void perform_mutation(MiniZinc::Call* call, std::span<const MiniZinc::ASTString> calls);
    std::uint64_t mutation_Call_count {};
};

MutationModel::Mutator::Mutator(MutationModel& mutation_model) :
    m_mutation_model { mutation_model } { }

void MutationModel::Mutator::vBinOp(MiniZinc::BinOp* binOp)
{
    logd("vBinOP: Detected operation {}", binOp->opToString().c_str());

    std::span<const MiniZinc::BinOpType> operators {};

    if (std::ranges::contains(relational_operators, binOp->op()))
        operators = relational_operators;
    else if (std::ranges::contains(arithmetic_operators, binOp->op()))
        operators = arithmetic_operators;

    if (operators.empty())
        logd("MutationModel::Mutator::vBinOp: Undetected mutation type");
    else
        perform_mutation(binOp, operators);
}

void MutationModel::Mutator::vUnOp(MiniZinc::UnOp* unOp)
{
    logd("vUnOp: Detected operation {}", unOp->opToString().c_str());

    if (std::ranges::contains(unary_operators, unOp->op()))
        perform_mutation(unOp, unary_operators);
    else
        logd("MutationModel::Mutator::vUnOp: Undetected mutation type");
}

void MutationModel::Mutator::vCall(MiniZinc::Call* call)
{
    logd("vCall: Detected call to {}", call->id().c_str());

    if (std::ranges::contains(calls, call->id()))
        perform_mutation(call, calls);
    else
        logd("MutationModel::Mutator::vCall: Unhandled call operation");
}

void MutationModel::Mutator::perform_mutation(MiniZinc::BinOp* op, std::span<const MiniZinc::BinOpType> operators)
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

        m_mutation_model.save_current_model("BinOP", mutation_BinOp_count++, occurrence_id++);
    }

    // Go back to the original for the next iteration.
    new (op) MiniZinc::BinOp(loc, op->lhs(), original_operator, op->rhs());
}

void MutationModel::Mutator::perform_mutation(MiniZinc::UnOp* op, std::span<const MiniZinc::UnOpType> operators)
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

        m_mutation_model.save_current_model("UnOP", mutation_UnOp_count++, occurrence_id++);
    }

    // Go back to the original for the next iteration.
    new (op) MiniZinc::UnOp(loc, original_operator, op->e());
}

void MutationModel::Mutator::perform_mutation(MiniZinc::Call* call, std::span<const MiniZinc::ASTString> calls)
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

        m_mutation_model.save_current_model("CALL", mutation_Call_count++, occurrence_id++);
    }

    call->id(original_call);
}

MutationModel::MutationModel(const std::filesystem::path& path) :
    m_model_path { std::filesystem::absolute(path) }
{
    if (!std::filesystem::is_regular_file(m_model_path))
        throw std::runtime_error { "Could not open the requested file." };

    if (!m_model_path.has_stem())
        throw std::runtime_error { "Could not determine the filename without extension of the model." };

    m_filename_stem = m_model_path.stem().generic_string();
}

MutationModel::MutationModel(const std::filesystem::path& path, std::string_view output_directory) :
    MutationModel { path }
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
        throw std::runtime_error { std::format(R"(Folder "{:s}" does not exist.)", m_mutation_folder_path.string()) };

    for (const auto& entry : std::filesystem::directory_iterator { m_mutation_folder_path })
        if (entry.is_directory() || entry.path().extension() != EXTENSION || !entry.path().filename().string().starts_with(m_filename_stem))
            throw std::runtime_error { R"(One or more elements inside the selected path are not models or mutants from the specified model. Cannot automatically remove the output folder.)" };

    std::filesystem::remove_all(m_mutation_folder_path);
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

    const auto mutant = std::format("{:s}-{:s}-{:d}-{:d}", m_filename_stem, mutant_name, mutant_id, occurrence_id);

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
            throw std::runtime_error { std::format(R"(A mutant with the path "{:s}" already exists. This shouldn't happen.)", path.string()) };

        auto file = std::ofstream(path);

        if (!file.is_open())
            throw std::runtime_error { std::format(R"(Could not open the mutant file "{:s}")", path.string()) };

        MiniZinc::Printer printer(file, WIDTH_PRINTER, false);
        printer.print(m_model);
    }
}

void MutationModel::run_mutants(const boost::filesystem::path& compiler_path, std::span<std::string_view> compiler_arguments) const
{
    if (m_memory.empty() && !std::filesystem::is_directory(m_mutation_folder_path))
        throw std::runtime_error { std::format(R"(Folder "{:s}" does not exist.)", m_mutation_folder_path.string()) };

    if (m_mutation_folder_path.empty() && m_memory.size() == 1)
        throw std::runtime_error { "Couldn't run any mutants because there aren't any." };

    boost::asio::io_context ctx;

    std::vector<boost::string_view> program_arguments;
    program_arguments.reserve(compiler_arguments.size() + 1);

    static constexpr boost::string_view stdin_argument { "-" };
    const auto model_path = m_model_path.string();

    program_arguments.emplace_back(m_memory.empty() ? model_path : stdin_argument);

    for (const auto argument : compiler_arguments)
        program_arguments.emplace_back(argument.begin(), argument.size());

    const auto [original_program_exit_code, original_program_result] = run_program(ctx, compiler_path, program_arguments, m_memory.empty() ? std::string {} : m_memory.front().second);

    if (original_program_exit_code != EXIT_SUCCESS)
        throw std::runtime_error { std::format("run: Could not execute the original model:\n{:s}", original_program_result) };

    std::uint64_t n_error {}, n_lives {}, n_dies {};

    const auto handle_output = [&original_program_result, &n_error, &n_lives, &n_dies](std::string_view mutant_name, int mutant_exit_code, std::string_view mutant_output)
    {
        std::string_view result_string;

        if (mutant_exit_code != EXIT_SUCCESS)
        {
            ++n_error;
            result_string = "ERROR";
        }
        else if (mutant_output == original_program_result)
        {
            ++n_lives;
            result_string = "LIVES";
        }
        else
        {
            ++n_dies;
            result_string = "DIES";
        }

        std::println("{:<20} {:<30}", mutant_name, result_string);
    };

    if (m_memory.empty())
    {
        for (const auto& entry : std::filesystem::directory_iterator { m_mutation_folder_path })
        {
            const auto entry_stem = entry.path().stem();

            if (entry_stem == m_filename_stem)
                continue;

            if (entry.is_directory() || entry.path().extension() != EXTENSION || !entry.path().filename().string().starts_with(m_filename_stem))
                throw std::runtime_error { "One or more elements inside the selected path are not models or mutants from the specified model. Can't run the mutants." };

            const auto mutant_path = std::filesystem::absolute(entry).string();

            program_arguments.front() = mutant_path;

            const auto [mutant_exit_code, mutant_output] = run_program(ctx, compiler_path, program_arguments);

            handle_output(entry_stem.string(), mutant_exit_code, mutant_output);
        }
    }
    else
    {
        for (const auto& [mutant_name, model] : m_memory | std::views::drop(1))
        {
            const auto [mutant_exit_code, mutant_output] = run_program(ctx, compiler_path, program_arguments, model);

            handle_output(mutant_name, mutant_exit_code, mutant_output);
        }
    }

    std::println("\n{2:s}{3:s}Summary:{0:s}\n  Error:  {1:s}{4:d}{0:s}\n  Living: {1:s}{5:d}{0:s}\n  Dead:   {1:s}{6:d}{0:s}", logging::code(logging::Style::Reset), logging::code(logging::Color::Blue), logging::code(logging::Style::Bold), logging::code(logging::Style::Underline), n_error, n_lives, n_dies);
}