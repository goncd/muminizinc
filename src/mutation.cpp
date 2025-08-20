#include <mutation.hpp>

#include <algorithm>   // std::ranges::find
#include <array>       // std::array, std::end
#include <cstdint>     // std::uint64_t
#include <filesystem>  //
#include <format>      // std::format
#include <fstream>     // std::fstream
#include <iostream>    // std::cerr, std::cout
#include <print>       // std::println
#include <span>        // std::span
#include <stdexcept>   // std::runtime_error
#include <string>      // std::string
#include <string_view> // std::string_view
#include <utility>

#include <minizinc/ast.hh>           // MiniZinc::BinOp, MiniZinc::BinOpType, MiniZinc::ConstraintI, MiniZinc::EVisitor, MiniZinc::Expression, MiniZinc::OutputI, MiniZinc::SolveI
#include <minizinc/astiterator.hh>   // MiniZinc::top_down
#include <minizinc/aststring.hh>     //
#include <minizinc/model.hh>         // MiniZinc::Env
#include <minizinc/parser.hh>        // MiniZinc::parse
#include <minizinc/prettyprinter.hh> // MiniZinc::Printer

static constexpr std::array relational_operators {
    MiniZinc::BinOpType::BOT_LE,
    MiniZinc::BinOpType::BOT_LQ,
    MiniZinc::BinOpType::BOT_GR,
    MiniZinc::BinOpType::BOT_GQ,
    MiniZinc::BinOpType::BOT_EQ,
    MiniZinc::BinOpType::BOT_NQ,
};

static constexpr std::array arithmetic_operators {
    MiniZinc::BinOpType::BOT_PLUS,
    MiniZinc::BinOpType::BOT_MINUS,
    MiniZinc::BinOpType::BOT_MULT,
    MiniZinc::BinOpType::BOT_DIV,
    MiniZinc::BinOpType::BOT_IDIV,
    MiniZinc::BinOpType::BOT_MOD,
    MiniZinc::BinOpType::BOT_POW,
};

static constexpr std::array unary_operators {
    MiniZinc::UnOpType::UOT_MINUS,
    MiniZinc::UnOpType::UOT_PLUS,
    // MiniZinc::UnOpType::UOT_NOT, (do we want to use this one?)
};

static const std::array calls {
    MiniZinc::Constants::constants().ids.forall,
    MiniZinc::Constants::constants().ids.exists,
};

class MutationModel::Mutator : public MiniZinc::EVisitor
{
public:
    explicit Mutator(MutationModel& mutation_model);

    bool enter(MiniZinc::Expression* expression)
    {
        if (auto* binOP = MiniZinc::Expression::dynamicCast<MiniZinc::BinOp>(expression))
            detect_type_mutation(binOP);
        else if (auto* unOP = MiniZinc::Expression::dynamicCast<MiniZinc::UnOp>(expression))
            detect_type_mutation(unOP);
        else if (auto* call = MiniZinc::Expression::dynamicCast<MiniZinc::Call>(expression))
            detect_type_mutation(call);
        else
            std::println("Undetected expression {}", std::to_underlying(MiniZinc::Expression::eid(expression)));

        return true;
    }

private:
    MutationModel& m_mutation_model;

    void detect_type_mutation(MiniZinc::BinOp* op);
    void detect_type_mutation(MiniZinc::UnOp* op);
    void detect_type_mutation(MiniZinc::Call* call);

    void perform_mutation(MiniZinc::BinOp* op, std::span<const MiniZinc::BinOpType> operators);
    std::uint64_t mutation_BinOp_count {};

    void perform_mutation(MiniZinc::UnOp* op, std::span<const MiniZinc::UnOpType> operators);
    std::uint64_t mutation_UnOp_count {};

    void perform_mutation(MiniZinc::Call* call, std::span<const MiniZinc::ASTString> calls);
    std::uint64_t mutation_Call_count {};
};

MutationModel::Mutator::Mutator(MutationModel& mutation_model) :
    m_mutation_model { mutation_model } { }

void MutationModel::Mutator::detect_type_mutation(MiniZinc::BinOp* op)
{
    std::println("Detected BINOP {}", op->opToString().c_str());

    if (std::ranges::find(relational_operators, op->op()) != std::end(relational_operators))
        perform_mutation(op, relational_operators);
    else if (std::ranges::find(arithmetic_operators, op->op()) != std::end(arithmetic_operators))
        perform_mutation(op, arithmetic_operators);
    else
        std::println(std::cerr, "Undetected mutation type");
}

void MutationModel::Mutator::detect_type_mutation(MiniZinc::UnOp* op)
{
    std::println("Detected UNOP {}", op->opToString().c_str());

    if (std::ranges::find(unary_operators, op->op()) != std::end(unary_operators))
        perform_mutation(op, unary_operators);
    else
        std::println(std::cerr, "Undetected mutation type");
}

void MutationModel::Mutator::detect_type_mutation(MiniZinc::Call* call)
{
    std::println("Detected CallOP {}", call->id().c_str());

    if (std::ranges::find(calls, call->id()) != std::end(calls))
        perform_mutation(call, calls);
    else
        std::println(std::cerr, "Undetected mutation type");
}

void MutationModel::Mutator::perform_mutation(MiniZinc::BinOp* op, std::span<const MiniZinc::BinOpType> operators)
{
    const auto original_operator = op->op();
    const auto& loc = MiniZinc::Expression::loc(op);

    const auto& lhs = MiniZinc::Expression::loc(op->lhs());
    const auto& rhs = MiniZinc::Expression::loc(op->rhs());

    std::println("Mutate detected at {}-{} to {}-{}\n\tLHS: {}-{} to {}-{}\n\tRHS: {}-{} to {}-{}",
        loc.firstLine(), loc.firstColumn(), loc.lastLine(), loc.lastColumn(),
        lhs.firstLine(), lhs.firstColumn(), lhs.lastLine(), lhs.lastColumn(),
        rhs.firstLine(), rhs.firstColumn(), rhs.lastLine(), rhs.lastColumn());

    std::uint64_t occurrence_id {};

    for (auto candidate_operator : operators)
    {
        if (original_operator == candidate_operator)
            continue;

        new (op) MiniZinc::BinOp(loc, op->lhs(), candidate_operator, op->rhs());

        std::println("Candidate is {}", op->opToString().c_str());

        m_mutation_model.save_current_model("BinOP", mutation_BinOp_count++, occurrence_id++);
    }

    // Go back to the original for the next iteration.
    new (op) MiniZinc::BinOp(loc, op->lhs(), original_operator, op->rhs());
}

void MutationModel::Mutator::perform_mutation(MiniZinc::UnOp* op, std::span<const MiniZinc::UnOpType> operators)
{
    const auto original_operator = op->op();
    const auto& loc = MiniZinc::Expression::loc(op);

    std::println("Mutate detected at {}-{} to {}-{}",
        loc.firstLine(), loc.firstColumn(), loc.lastLine(), loc.lastColumn());

    std::uint64_t occurrence_id {};

    for (auto candidate_operator : operators)
    {
        if (original_operator == candidate_operator)
            continue;

        new (op) MiniZinc::UnOp(loc, candidate_operator, op->e());

        std::println("Candidate is {}", op->opToString().c_str());

        m_mutation_model.save_current_model("UnOP", mutation_UnOp_count++, occurrence_id++);
    }

    // Go back to the original for the next iteration.
    new (op) MiniZinc::UnOp(loc, original_operator, op->e());
}

void MutationModel::Mutator::perform_mutation(MiniZinc::Call* call, std::span<const MiniZinc::ASTString> calls)
{
    const auto original_call = call->id();
    const auto& loc = MiniZinc::Expression::loc(call);

    std::println("Mutate detected at {}-{} to {}-{} -> {}",
        loc.firstLine(), loc.firstColumn(), loc.lastLine(), loc.lastColumn(), call->id().c_str());

    std::uint64_t occurrence_id {};

    for (const auto& candidate_call : calls)
    {
        if (original_call == candidate_call)
            continue;

        std::println("\tMutate {} to {}", original_call.c_str(), candidate_call.c_str());

        call->id(candidate_call);

        m_mutation_model.save_current_model("CALL", mutation_Call_count++, occurrence_id++);
    }

    call->id(original_call);
}

MutationModel::MutationModel(std::filesystem::path path, std::string_view output_directory) :
    m_model_path { std::move(path) }
{
    if (!std::filesystem::is_regular_file(m_model_path))
        throw std::runtime_error { "Could not open the requested file." };

    // Create the folder that will hold all the mutant code.
    // First, we need to determine the name of the folder, which will have the following format:
    // If the given path is /test_file.mzn, then the folder will be /test_file/.
    //
    // If a folder with such name exists, then we're OK as long as it's empty. If it has contents, there might be
    // code from an old analysis.
    // Of course, we'll create the folder if it doesn't already exist.
    if ((output_directory.empty() && !m_model_path.has_relative_path()))
        throw std::runtime_error { "Could not automatically determine the path for storing the mutants." };

    if (!m_model_path.has_stem())
        throw std::runtime_error { "Could not determine the filename without extension of the model." };

    m_filename_stem = m_model_path.stem().generic_string();

    m_mutation_folder_path = std::filesystem::absolute(output_directory.empty() ? m_model_path.parent_path() / std::format("{:s}-mutants", m_filename_stem) : output_directory);
}

void MutationModel::clear_output_folder()
{
    if (!std::filesystem::is_directory(m_mutation_folder_path))
        throw std::runtime_error { std::format(R"(Folder "{:s}" does not exist.)", m_mutation_folder_path.string()) };

    for (const auto& entry : std::filesystem::directory_iterator { m_mutation_folder_path })
        if (entry.is_directory() || entry.path().extension() != EXTENSION || !entry.path().filename().string().starts_with(m_filename_stem))
            throw std::runtime_error { "One or more elements inside the selected path are not models or mutants. Can't automatically remove the output folder." };

    std::filesystem::remove_all(m_mutation_folder_path);
}

void MutationModel::find_mutants()
{
    MiniZinc::Env env;

    m_model = MiniZinc::parse(env, { m_model_path }, {}, "", "", {}, {}, false, true, false, true, std::cerr);

    if (std::filesystem::is_directory(m_mutation_folder_path))
    {
        if (!std::filesystem::is_empty(m_mutation_folder_path))
            throw std::runtime_error { std::format(R"(The selected path for storing the mutants, `{:s}`, is non-empty. Please run `clean` first or manually remove or empty the folder to avoid accidental data loss.)", m_mutation_folder_path.generic_string()) };
    }
    else
        std::filesystem::create_directory(m_mutation_folder_path);

    // Print the original file passed through the compiler to strip out any comments so we can
    // diff from it cleanly.
    save_current_model({}, 0, 0);

    Mutator mutator { *this };

    for (auto* item : *m_model)
    {
        if (const auto* constraintI = item->dynamicCast<MiniZinc::ConstraintI>())
            MiniZinc::top_down(mutator, constraintI->e());
        else if (const auto* solveI = item->dynamicCast<MiniZinc::SolveI>(); solveI != nullptr && solveI->e() != nullptr)
            MiniZinc::top_down(mutator, solveI->e());
        else if (const auto* outputI = item->dynamicCast<MiniZinc::OutputI>())
            MiniZinc::top_down(mutator, outputI->e());
    }
}

void MutationModel::save_current_model(std::string_view mutant_name, std::uint64_t mutant_id, std::uint64_t occurrence_id) const
{
    if (m_model == nullptr)
        throw std::runtime_error { "There is no model to print." };

    const auto path = m_mutation_folder_path / (mutant_name.empty() ? std::format("{:s}{:s}", m_filename_stem, EXTENSION) : std::format("{:s}-{:s}-{:d}-{:d}{:s}", m_filename_stem, mutant_name, mutant_id, occurrence_id, EXTENSION));

    if (std::filesystem::exists(path))
        throw std::runtime_error { std::format(R"(A mutant with the path "{:s}" already exists. This shouldn't happen.)", path.string()) };

    auto file = std::ofstream(path);

    if (!file.is_open())
        throw std::runtime_error { std::format(R"(Could not open the mutant file "{:s}")", path.string()) };

    MiniZinc::Printer printer(file, WIDTH_PRINTER, false);

    printer.print(m_model);
}

void MutationModel::run_mutants()
{
}