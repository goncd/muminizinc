#include <operators/mutator.hpp>

#include <algorithm>   // std::ranges::equal, std::ranges::next_permutation, std::ranges::sort
#include <array>       // std::array
#include <cstdint>     // std::uint64_t
#include <format>      // std::format
#include <span>        // std::span
#include <string_view> // std::string_view
#include <utility>     // std::pair
#include <vector>      // std::vector

#include <minizinc/ast.hh>           // MiniZinc::BinOpType, MiniZinc::Expression

#include <build/config.hpp>            // MuMiniZinc::config::is_debug_build
#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <logging.hpp>                 // logd, logging::code, logging::Color, logging::Style

namespace
{

using namespace std::string_view_literals;

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

constexpr auto unary_operators_name { "UNA"sv };

const std::array calls {
    MiniZinc::Constants::constants().ids.forall,
    MiniZinc::Constants::constants().ids.exists,
};

constexpr auto call_name { "CALL"sv };
constexpr auto call_swap_name { "SWP"sv };

constexpr std::array mutant_help {
    std::pair { relational_operators_name, "Relational operators"sv },
    std::pair { arithmetic_operators_name, "Arithmetic operators"sv },
    std::pair { unary_operators_name, "Unary operators removal"sv },
    std::pair { call_name, "Function calls"sv },
    std::pair { call_swap_name, "Function call argument swap"sv }
};

};

namespace MuMiniZinc
{

[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>> get_available_operators()
{
    return mutant_help;
}

void throw_if_invalid_operators(std::span<const ascii_ci_string_view> allowed_operators)
{
    const auto valid_operators { MuMiniZinc::get_available_operators() };

    for (const auto mutant : allowed_operators)
    {
        bool found = false;

        for (const auto& [name, _] : valid_operators)
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

void Mutator::vBinOp(MiniZinc::BinOp* binOp)
{
    logd("vBinOP: Detected operation {}", binOp->opToString().c_str());

    if (m_allowed_operators.empty() || std::ranges::contains(m_allowed_operators, ascii_ci_string_view { unary_operators_name }))
        perform_mutation_unop(binOp, unary_operators_name);

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
    else if (m_allowed_operators.empty() || std::ranges::contains(m_allowed_operators, ascii_ci_string_view { operator_name }))
        perform_mutation(binOp, operators, operator_name);
}

void Mutator::vCall(MiniZinc::Call* call)
{
    logd("vCall: Detected call to {}", call->id().c_str());

    if (m_allowed_operators.empty() || std::ranges::contains(m_allowed_operators, ascii_ci_string_view { unary_operators_name }))
        perform_mutation_unop(call, unary_operators_name);

    if (m_allowed_operators.empty() || std::ranges::contains(m_allowed_operators, ascii_ci_string_view { call_swap_name }))
        perform_call_swap_mutation(call);

    if (std::ranges::contains(calls, call->id()) && (m_allowed_operators.empty() || std::ranges::contains(m_allowed_operators, ascii_ci_string_view { call_name })))
        perform_mutation(call, calls, call_name);
    else
        logd("MutationModel::Mutator::vCall: Unhandled call operation");
}

void Mutator::perform_mutation(MiniZinc::BinOp* op, std::span<const MiniZinc::BinOpType> operators, std::string_view operator_name)
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

        m_entries.save_model(m_model, operator_name, occurrence_id++, m_detected_enums);
    }

    // Go back to the original for the next iteration.
    new (op) MiniZinc::BinOp(loc, op->lhs(), original_operator, op->rhs());
}

void Mutator::perform_mutation_unop(MiniZinc::BinOp* op, std::string_view operator_name)
{
    auto* const lhs = op->lhs();
    auto* const rhs = op->rhs();

    std::uint64_t occurrence_id {};

    if (auto* unop = MiniZinc::Expression::dynamicCast<MiniZinc::UnOp>(lhs))
    {
        op->lhs(unop->e());
        m_entries.save_model(m_model, operator_name, occurrence_id++, m_detected_enums);
        op->lhs(lhs);
    }

    if (auto* unop = MiniZinc::Expression::dynamicCast<MiniZinc::UnOp>(rhs))
    {
        op->rhs(unop->e());
        m_entries.save_model(m_model, operator_name, occurrence_id++, m_detected_enums);
        op->rhs(rhs);
    }
}

void Mutator::perform_mutation_unop(MiniZinc::Call* call, std::string_view operator_name)
{
    std::uint64_t occurrence_id {};

    for (unsigned int i {}; i < call->argCount(); ++i)
    {
        auto* const original_element = call->arg(i);

        if (auto* unop = MiniZinc::Expression::dynamicCast<MiniZinc::UnOp>(original_element))
        {
            call->arg(i, unop->e());
            m_entries.save_model(m_model, operator_name, occurrence_id++, m_detected_enums);
            call->arg(i, original_element);
        }
    }
}

void Mutator::perform_mutation(MiniZinc::Call* call, std::span<const MiniZinc::ASTString> calls, std::string_view operator_name)
{
    const auto original_call = call->id();

    if constexpr (MuMiniZinc::config::is_debug_build)
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

        m_entries.save_model(m_model, operator_name, occurrence_id++, m_detected_enums);
    }

    call->id(original_call);
}

void Mutator::perform_call_swap_mutation(MiniZinc::Call* call)
{
    if (call->argCount() <= 1)
        return;

    logd("Mutating argument order of call to {:s}.", call->id().c_str());

    const auto calls { call->args() };
    const std::vector original(calls.begin(), calls.end());

    auto permutation { original };

    std::ranges::sort(permutation);

    std::uint64_t occurrence_id {};

    do
    {
        if (std::ranges::equal(permutation, original))
            continue;

        call->args(permutation);

        m_entries.save_model(m_model, call_swap_name, occurrence_id++, m_detected_enums);

    } while (std::ranges::next_permutation(permutation).found);

    call->args(original);
}

}