#include <mutation.hpp>

#include <algorithm>   // std::ranges::find
#include <array>       // std::array, std::end
#include <cstdint>     // std::uint64_t
#include <format>      // std::format
#include <fstream>     // std::ofstream
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

class Mutator : public MiniZinc::EVisitor
{
public:
    explicit Mutator(MutationModel& mutation_model);

    bool enter(MiniZinc::Expression* e)
    {
        if (auto binOP = MiniZinc::Expression::dynamicCast<MiniZinc::BinOp>(e))
            detect_type_mutation(binOP);

        return true;
    }

private:
    MutationModel& m_mutation_model;

    void detect_type_mutation(MiniZinc::BinOp* op);
    void perform_mutation(MiniZinc::BinOp* op, std::span<const MiniZinc::BinOpType> operators);

    std::uint64_t m_id {};
};

Mutator::Mutator(MutationModel& mutation_model) :
    m_mutation_model { mutation_model } { }

void Mutator::detect_type_mutation(MiniZinc::BinOp* op)
{
    std::println("Detected {}", op->opToString().c_str());

    if (std::ranges::find(relational_operators, op->op()) != std::end(relational_operators))
        perform_mutation(op, relational_operators);
    else if (std::ranges::find(arithmetic_operators, op->op()) != std::end(arithmetic_operators))
        perform_mutation(op, arithmetic_operators);
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

        new (op) MiniZinc::BinOp(op->loc(op), op->lhs(), candidate_operator, op->rhs());

        std::println("Candidate is {}", op->opToString().c_str());

        const auto filename = std::format("{}-{}-{}-{}", loc.filename().c_str(), loc.firstLine(), loc.firstColumn(), ++m_id);

        auto file = std::ofstream(filename);

        if (!file.is_open())
            throw std::runtime_error { std::format(R"(Could not open the mutant file "{:s}")", filename) };

        MiniZinc::Printer p(file);

        p.print(m_mutation_model.model());
    }

    // Go back to the original for the next iteration.
    new (op) MiniZinc::BinOp(op->loc(op), op->lhs(), original_operator, op->rhs());
}

MutationModel::MutationModel(std::string_view path) :
    m_path { path }
{
    MiniZinc::Env env;

    m_model = MiniZinc::parse(env, { std::string(path) }, {}, "", "", {}, {}, false, true, false, true, std::cout);

    /*MiniZinc::Printer p(std::cout);
    p.print(m_model);
    exit(1);*/
}

void MutationModel::find_mutants()
{
    Mutator mutator { *this };

    for (auto a : *m_model)
    {
        if (const auto* i = a->dynamicCast<MiniZinc::ConstraintI>())
            MiniZinc::top_down(mutator, i->e());
        else if (const auto* i = a->dynamicCast<MiniZinc::SolveI>(); i != nullptr && i->e() != nullptr)
            MiniZinc::top_down(mutator, i->e());
        else if (const auto* i = a->dynamicCast<MiniZinc::OutputI>())
            MiniZinc::top_down(mutator, i->e());
    }
}
