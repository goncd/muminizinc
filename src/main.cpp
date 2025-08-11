#include <minizinc/ast.hh>
#include <minizinc/astiterator.hh>
#include <minizinc/solver.hh>

#include <fstream>
#include <ostream>
#include <print>
#include <span>
#include <string>
#include <vector>

namespace
{

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

MiniZinc::Model* model;

static std::uint64_t id = 0;

void perform_mutation(MiniZinc::BinOp* op, std::span<const MiniZinc::BinOpType> operators)
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

        const auto filename = std::format("{}-{}-{}-{}", loc.filename().c_str(), loc.firstLine(), loc.firstColumn(), ++id);

        auto file = std::ofstream(filename);

        assert(file.is_open());

        MiniZinc::Printer p(file);

        p.print(model);
    }

    // Go back to the original for the next iteration.
    new (op) MiniZinc::BinOp(op->loc(op), op->lhs(), original_operator, op->rhs());
}

void detect_type_mutation(MiniZinc::BinOp* op)
{
    std::println("Detected {}", op->opToString().c_str());

    if (std::ranges::find(relational_operators, op->op()) != std::end(relational_operators))
        perform_mutation(op, relational_operators);
    else if (std::ranges::find(arithmetic_operators, op->op()) != std::end(arithmetic_operators))
        perform_mutation(op, arithmetic_operators);
    else
        std::println(std::cerr, "Undetected mutation type");
}

}

class Mutator : public MiniZinc::EVisitor
{
public:
    bool enter(MiniZinc::Expression* e)
    {
        if (auto binOP = MiniZinc::Expression::dynamicCast<MiniZinc::BinOp>(e))
            detect_type_mutation(binOP);

        return true;
    }
};

// Mutator
/*class Mutator : public MiniZinc::ItemVisitor
{
public:
    void vConstraintI(MiniZinc::ConstraintI* c)
    {
        std::println("Constraint.");
        if (MiniZinc::Expression::isa<MiniZinc::BinOp>(c->e()))
        {
            auto binOP = MiniZinc::Expression::cast<MiniZinc::BinOp>(c->e());

            *binOP = MiniZinc::BinOp(binOP->loc(binOP), binOP->lhs(), MiniZinc::BinOpType::BOT_EQ, binOP->rhs());

            std::println("Detectado {}", binOP->opToString().c_str());
        }
    }
};*/

int main()
try
{
    MiniZinc::Env env;

    std::vector<std::string> includes;

    // TODO: Add library
    /*std::string share_dir = MiniZinc::FileUtils::share_directory();
    if (share_dir.empty())
    {
        std::println(std::cerr, "Lib not found");
        return 1;
    }

    std::string mzn_stdlib_dir = share_dir + "/std/";
    includes.push_back(mzn_stdlib_dir);*/

    model = MiniZinc::parse(env, { "loan.mzn" }, {}, "", "", includes, {}, false, true, false, true, std::cout);

    Mutator m;
    
    for (auto a : *model)
    {
        if (const auto* i = a->dynamicCast<MiniZinc::ConstraintI>())
            MiniZinc::top_down(m, i->e());
        else if (const auto* i = a->dynamicCast<MiniZinc::SolveI>(); i != nullptr && i->e() != nullptr)
            MiniZinc::top_down(m, i->e());
        else if (const auto* i = a->dynamicCast<MiniZinc::OutputI>())
            MiniZinc::top_down(m, i->e());
    }
}
catch (const MiniZinc::Error& e)
{
    std::println(std::cerr, "Error \"{}\": {}", e.what(), e.msg());

    return 1;
}
catch (const std::exception& e)
{
    std::println(std::cerr, "Error: {}", e.what());

    return 1;
}
catch (...)
{
    std::println(std::cerr, "Unknown error");

    return 1;
}
