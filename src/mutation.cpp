#include <filesystem>
#include <minizinc/aststring.hh>
#include <mutation.hpp>

#include <algorithm>   // std::ranges::find
#include <array>       // std::array, std::end
#include <cstdint>     // std::uint64_t
#include <format>      // std::format
#include <fstream>     // std::fstream
#include <iostream>    // std::cerr, std::cout
#include <print>       // std::println
#include <span>        // std::span
#include <stdexcept>   // std::runtime_error
#include <string>      // std::string
#include <string_view> // std::string_view

#include <minizinc/ast.hh>           // MiniZinc::BinOp, MiniZinc::BinOpType, MiniZinc::ConstraintI, MiniZinc::EVisitor, MiniZinc::Expression, MiniZinc::OutputI, MiniZinc::SolveI
#include <minizinc/astiterator.hh>   // MiniZinc::top_down
#include <minizinc/model.hh>         // MiniZinc::Env
#include <minizinc/parser.hh>        // MiniZinc::parse
#include <minizinc/prettyprinter.hh> // MiniZinc::Printer
#include <utility>

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

class Mutator : public MiniZinc::EVisitor
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
    void perform_mutation(MiniZinc::UnOp* op, std::span<const MiniZinc::UnOpType> operators);
    void perform_mutation(MiniZinc::Call* call, std::span<const MiniZinc::ASTString> calls);

    std::uint64_t m_id {};
};

Mutator::Mutator(MutationModel& mutation_model) :
    m_mutation_model { mutation_model } { }

void Mutator::detect_type_mutation(MiniZinc::BinOp* op)
{
    std::println("Detected BINOP {}", op->opToString().c_str());

    if (std::ranges::find(relational_operators, op->op()) != std::end(relational_operators))
        perform_mutation(op, relational_operators);
    else if (std::ranges::find(arithmetic_operators, op->op()) != std::end(arithmetic_operators))
        perform_mutation(op, arithmetic_operators);
    else
        std::println(std::cerr, "Undetected mutation type");
}

void Mutator::detect_type_mutation(MiniZinc::UnOp* op)
{
    std::println("Detected UNOP {}", op->opToString().c_str());

    if (std::ranges::find(unary_operators, op->op()) != std::end(unary_operators))
        perform_mutation(op, unary_operators);
    else
        std::println(std::cerr, "Undetected mutation type");
}

void Mutator::detect_type_mutation(MiniZinc::Call* call)
{
    std::println("Detected CallOP {}", call->id().c_str());

    if (std::ranges::find(calls, call->id()) != std::end(calls))
        perform_mutation(call, calls);
    else
        std::println(std::cerr, "Undetected mutation type");
}

void Mutator::perform_mutation(MiniZinc::BinOp* op, std::span<const MiniZinc::BinOpType> operators)
{
    const auto original_operator = op->op();
    const auto& loc = MiniZinc::Expression::loc(op);

    const auto& lhs = MiniZinc::Expression::loc(op->lhs());
    const auto& rhs = MiniZinc::Expression::loc(op->rhs());

    std::println("Mutate detected at {}-{} to {}-{}\n\tLHS: {}-{} to {}-{}\n\tRHS: {}-{} to {}-{}",
        loc.firstLine(), loc.firstColumn(), loc.lastLine(), loc.lastColumn(),
        lhs.firstLine(), lhs.firstColumn(), lhs.lastLine(), lhs.lastColumn(),
        rhs.firstLine(), rhs.firstColumn(), rhs.lastLine(), rhs.lastColumn());

    for (auto candidate_operator : operators)
    {
        if (original_operator == candidate_operator)
            continue;

        new (op) MiniZinc::BinOp(loc, op->lhs(), candidate_operator, op->rhs());

        std::println("Candidate is {}", op->opToString().c_str());

        const auto filename = std::format("{}/{}-{}-{}-{}.mzn", m_mutation_model.mutation_folder_path(), m_mutation_model.mutation_original_filename_stem(), loc.firstLine(), loc.firstColumn(), ++m_id);
        m_mutation_model.save_current_model(filename.c_str());
    }

    // Go back to the original for the next iteration.
    new (op) MiniZinc::BinOp(loc, op->lhs(), original_operator, op->rhs());
}

void Mutator::perform_mutation(MiniZinc::UnOp* op, std::span<const MiniZinc::UnOpType> operators)
{
    const auto original_operator = op->op();
    const auto& loc = MiniZinc::Expression::loc(op);

    std::println("Mutate detected at {}-{} to {}-{}",
        loc.firstLine(), loc.firstColumn(), loc.lastLine(), loc.lastColumn());

    for (auto candidate_operator : operators)
    {
        if (original_operator == candidate_operator)
            continue;

        new (op) MiniZinc::UnOp(loc, candidate_operator, op->e());

        std::println("Candidate is {}", op->opToString().c_str());

        const auto filename = std::format("{}/{}-{}-{}-{}.mzn", m_mutation_model.mutation_folder_path(), m_mutation_model.mutation_original_filename_stem(), loc.firstLine(), loc.firstColumn(), ++m_id);
        m_mutation_model.save_current_model(filename.c_str());
    }

    // Go back to the original for the next iteration.
    new (op) MiniZinc::UnOp(loc, original_operator, op->e());
}

void Mutator::perform_mutation(MiniZinc::Call* call, std::span<const MiniZinc::ASTString> calls)
{
    const auto original_call = call->id();
    const auto& loc = MiniZinc::Expression::loc(call);

    std::println("Mutate detected at {}-{} to {}-{} -> {}",
        loc.firstLine(), loc.firstColumn(), loc.lastLine(), loc.lastColumn(), call->id().c_str());

    /*for (auto candidate_call : calls)
    {
        if (original_call == candidate_call)
            continue;

        std::println("\tMutate {} to {}", original_call.c_str(), candidate_call.c_str());

        call->id(candidate_call);

        const auto filename = std::format("{}/{}-{}-{}-{}.mzn", m_mutation_model.mutation_folder_path(), m_mutation_model.mutation_original_filename_stem(), loc.firstLine(), loc.firstColumn(), ++m_id);
        m_mutation_model.save_current_model(filename.c_str());
    }

    call->id(original_call);*/
}

MutationModel::MutationModel(std::string_view path)
{
    MiniZinc::Env env;

    m_model = MiniZinc::parse(env, { std::string(path) }, {}, "", "", {}, {}, false, true, false, true, std::cout);

    // Create the folder that will hold all the mutant code.
    // First, we need to determine the name of the folder, which will have the following format:
    // If the given path is /test_file.mzn, then the folder will be /test_file/.
    //
    // If a folder with such name exists, then we're OK as long as it's empty. If it has contents, there might be
    // code from an old analysis.
    // Of course, we'll create the folder if it doesn't already exist.
    const std::filesystem::path given_path { path };

    if (!given_path.has_relative_path() || !given_path.has_stem())
        throw std::runtime_error { "Could not automatically determine the path for storing the mutants." };

    m_filename_stem = given_path.stem().generic_string();

    const std::filesystem::path mutation_folder_path = given_path.parent_path() / std::format("{:s}-mutants", m_filename_stem);

    if (std::filesystem::is_directory(mutation_folder_path))
    {
        if (!std::filesystem::is_empty(mutation_folder_path))
            throw std::runtime_error { std::format(R"(The selected path for storing the mutants, `{:s}`, is non-empty. Please run `clean` first.)", mutation_folder_path.generic_string()) };
    }
    else
        std::filesystem::create_directory(mutation_folder_path);

    m_mutation_folder_path = std::filesystem::absolute(mutation_folder_path);

    // Let's print the original file passed through the compiler to strip out any comments so we can
    // diff from it cleanly.
    save_current_model(std::format("{}/{}.mzn", m_mutation_folder_path, m_filename_stem).c_str());
}

/*void determine_folder_path_and_stem(const std::filesystem::path& path)
{

    m_filename_stem = path.stem().generic_string();

    if (!path.has_relative_path() || !path.has_stem())
        throw std::runtime_error { "Could not automatically determine the path for storing the mutants." };

    const std::filesystem::path mutation_folder_path = given_path.parent_path() / std::format("{:s}-mutants", m_filename_stem);
}*/

void MutationModel::find_mutants()
{
    Mutator mutator { *this };

    for (auto* item : *m_model)
    {
        if (const auto* i = item->dynamicCast<MiniZinc::ConstraintI>())
            MiniZinc::top_down(mutator, i->e());
        else if (const auto* i = item->dynamicCast<MiniZinc::SolveI>(); i != nullptr && i->e() != nullptr)
            MiniZinc::top_down(mutator, i->e());
        else if (const auto* i = item->dynamicCast<MiniZinc::OutputI>())
            MiniZinc::top_down(mutator, i->e());
    }
}

void MutationModel::save_current_model(const char* path)
{
    auto file = std::ofstream(path);

    if (!file.is_open())
        throw std::runtime_error { std::format(R"(Could not open the mutant file "{:s}")", path) };

    MiniZinc::Printer printer(file, 80, false);

    printer.print(m_model);
}
