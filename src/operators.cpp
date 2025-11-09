#include <muminizinc/operators.hpp>

#include <algorithm>   // std::ranges::contains, std::ranges::equal, std::ranges::next_permutation, std::ranges::sort
#include <array>       // std::array
#include <cstdint>     // std::uint64_t
#include <format>      // std::format
#include <span>        // std::span
#include <string_view> // std::string_view
#include <utility>     // std::pair
#include <vector>      // std::vector

#include <minizinc/ast.hh> // MiniZinc::BinOpType, MiniZinc::Expression

#include <muminizinc/build/config.hpp>            // MuMiniZinc::build::is_debug_build
#include <muminizinc/case_insensitive_string.hpp> // ascii_ci_string_view
#include <muminizinc/logging.hpp>                 // logd, logging::code, logging::Color, logging::Style
#include <muminizinc/mutation.hpp>                // MuMiniZinc::EntryResult

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

// Set operators that return a boolean
constexpr std::array set_operators_bool {
    MiniZinc::BinOpType::BOT_SUBSET,
    MiniZinc::BinOpType::BOT_SUPERSET
};

constexpr std::array set_operators {
    MiniZinc::BinOpType::BOT_UNION,
    MiniZinc::BinOpType::BOT_DIFF,
    MiniZinc::BinOpType::BOT_SYMDIFF,
    MiniZinc::BinOpType::BOT_INTERSECT,
};

constexpr std::array boolean_operators {
    MiniZinc::BinOpType::BOT_EQUIV,
    MiniZinc::BinOpType::BOT_IMPL,
    MiniZinc::BinOpType::BOT_RIMPL,
    MiniZinc::BinOpType::BOT_OR,
    MiniZinc::BinOpType::BOT_AND,
    MiniZinc::BinOpType::BOT_XOR,
};

constexpr std::array binary_operators_categories {
    std::pair { std::span<const MiniZinc::BinOpType> { relational_operators }, MuMiniZinc::available_operators[0].first },
    std::pair { std::span<const MiniZinc::BinOpType> { arithmetic_operators }, MuMiniZinc::available_operators[1].first },
    std::pair { std::span<const MiniZinc::BinOpType> { set_operators_bool }, MuMiniZinc::available_operators[2].first },
    std::pair { std::span<const MiniZinc::BinOpType> { set_operators }, MuMiniZinc::available_operators[2].first },
    std::pair { std::span<const MiniZinc::BinOpType> { boolean_operators }, MuMiniZinc::available_operators[3].first },
};

constexpr auto unary_operators_name { MuMiniZinc::available_operators[4].first };

constexpr auto call_name { MuMiniZinc::available_operators[5].first };
const std::array calls {
    MiniZinc::Constants::constants().ids.forall,
    MiniZinc::Constants::constants().ids.exists,
};

constexpr auto call_swap_name { MuMiniZinc::available_operators[6].first };

}

namespace MuMiniZinc
{

void Mutator::vBinOp(MiniZinc::BinOp* binOp)
{
    logd("vBinOP: Detected operator {}", binOp->opToString().c_str());

    const auto currently_detected_mutants { m_entries.mutants().size() };
    ++m_location_counter;

    if (m_allowed_operators.empty() || std::ranges::contains(m_allowed_operators, ascii_ci_string_view { unary_operators_name }))
        perform_mutation_unop(binOp);

    const auto detected_operator = binOp->op();
    std::span<const MiniZinc::BinOpType> operators;
    std::string_view operator_name;

    for (const auto& [set, name] : binary_operators_categories)
        if (std::ranges::contains(set, detected_operator))
        {
            operators = set;
            operator_name = name;

            break;
        }

    if (operators.empty())
        logd("vBinOp: Undetected mutation type");
    else if (m_allowed_operators.empty() || std::ranges::contains(m_allowed_operators, ascii_ci_string_view { operator_name }))
        perform_mutation(binOp, operators, operator_name);

    if (currently_detected_mutants == m_entries.mutants().size())
        --m_location_counter;
}

void Mutator::vCall(MiniZinc::Call* call)
{
    logd("vCall: Detected call to {}", call->id().c_str());

    const auto currently_detected_mutants { m_entries.mutants().size() };
    ++m_location_counter;

    if (m_allowed_operators.empty() || std::ranges::contains(m_allowed_operators, ascii_ci_string_view { unary_operators_name }))
        perform_mutation_unop(call);

    if (m_allowed_operators.empty() || std::ranges::contains(m_allowed_operators, ascii_ci_string_view { call_swap_name }))
        perform_call_swap_mutation(call);

    if (std::ranges::contains(calls, call->id()))
    {
        if (m_allowed_operators.empty() || std::ranges::contains(m_allowed_operators, ascii_ci_string_view { call_name }))
            perform_mutation(call, calls);
    }
    else
        logd("vCall: Unhandled call operation");

    if (currently_detected_mutants == m_entries.mutants().size())
        --m_location_counter;
}

void Mutator::perform_mutation(MiniZinc::BinOp* op, std::span<const MiniZinc::BinOpType> operators, std::string_view operator_name)
{
    const auto original_operator = op->op();
    const auto& loc = MiniZinc::Expression::loc(op);

    if constexpr (build::is_debug_build)
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

        m_entries.save_model(m_model, operator_name, m_location_counter, ++occurrence_id, m_detected_enums);
    }

    // Go back to the original for the next iteration.
    new (op) MiniZinc::BinOp(loc, op->lhs(), original_operator, op->rhs());
}

void Mutator::perform_mutation_unop(MiniZinc::BinOp* op)
{
    auto* const lhs = op->lhs();
    auto* const rhs = op->rhs();

    if (auto* unop = MiniZinc::Expression::dynamicCast<MiniZinc::UnOp>(lhs))
    {
        op->lhs(unop->e());
        m_entries.save_model(m_model, unary_operators_name, m_location_counter, 1, m_detected_enums);
        op->lhs(lhs);
    }

    if (auto* unop = MiniZinc::Expression::dynamicCast<MiniZinc::UnOp>(rhs))
    {
        op->rhs(unop->e());
        m_entries.save_model(m_model, unary_operators_name, m_location_counter, 1, m_detected_enums);
        op->rhs(rhs);
    }
}

void Mutator::perform_mutation_unop(MiniZinc::Call* call)
{
    for (unsigned int i {}; i < call->argCount(); ++i)
    {
        auto* const original_element = call->arg(i);

        if (auto* unop = MiniZinc::Expression::dynamicCast<MiniZinc::UnOp>(original_element))
        {
            call->arg(i, unop->e());
            m_entries.save_model(m_model, unary_operators_name, m_location_counter, 1, m_detected_enums);
            call->arg(i, original_element);
        }
    }
}

void Mutator::perform_mutation(MiniZinc::Call* call, std::span<const MiniZinc::ASTString> candidate_calls)
{
    const auto original_call = call->id();

    std::uint64_t occurrence_id {};

    for (const auto& candidate_call : candidate_calls)
    {
        if (original_call == candidate_call)
            continue;

        logd("Mutating from {} to {}", original_call.c_str(), candidate_call.c_str());

        call->id(candidate_call);

        m_entries.save_model(m_model, call_name, m_location_counter, ++occurrence_id, m_detected_enums);
    }

    call->id(original_call);
}

void Mutator::perform_call_swap_mutation(MiniZinc::Call* call)
{
    if (call->argCount() <= 1)
        return;

    logd("Mutating argument order of call to {:s}.", call->id().c_str());

    const auto call_arguments { call->args() };
    const std::vector original(call_arguments.begin(), call_arguments.end());

    auto permutation { original };

    std::ranges::sort(permutation);

    std::uint64_t occurrence_id {};

    do
    {
        if (std::ranges::equal(permutation, original))
            continue;

        call->args(permutation);

        m_entries.save_model(m_model, call_swap_name, m_location_counter, ++occurrence_id, m_detected_enums);

    } while (std::ranges::next_permutation(permutation).found);

    call->args(original);
}

} // namespace MuMiniZinc