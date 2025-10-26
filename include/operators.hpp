#ifndef OPERATORS_HPP
#define OPERATORS_HPP

#include <array>       // std::array
#include <span>        // std::span
#include <string_view> // std::string_view
#include <utility>     // std::pair

#include <minizinc/ast.hh> // MiniZinc::EVisitor

#include <case_insensitive_string.hpp> // ascii_ci_string_view

namespace MuMiniZinc
{

class EntryResult;

using namespace std::string_view_literals;

inline constexpr std::array available_operators {
    std::pair { "ROR"sv, "Relational operator replacement"sv },
    std::pair { "AOR"sv, "Arithmetic operator replacement"sv },
    std::pair { "SOR"sv, "Set operator replacement"sv },
    std::pair { "COR"sv, "Conditional operator replacement"sv },
    std::pair { "UOD"sv, "Unary operator deletion"sv },
    std::pair { "FCR"sv, "Function call replacement"sv },
    std::pair { "FAS"sv, "Function call argument swap"sv }
};

class UnknownOperator : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

void throw_if_invalid_operators(std::span<const ascii_ci_string_view> allowed_operators);

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
class Mutator : public MiniZinc::EVisitor
{
public:
    constexpr Mutator(const MiniZinc::Model* model, std::span<const ascii_ci_string_view> m_allowed_operators, MuMiniZinc::EntryResult& entries, std::span<const std::pair<std::string, std::string>> detected_enums) noexcept :
        m_model { model }, m_allowed_operators { m_allowed_operators }, m_entries { entries }, m_detected_enums { detected_enums } { }

    void vBinOp(MiniZinc::BinOp* binOp);

    void vCall(MiniZinc::Call* call);

private:
    const MiniZinc::Model* m_model;

    std::span<const ascii_ci_string_view> m_allowed_operators;

    MuMiniZinc::EntryResult& m_entries;

    std::span<const std::pair<std::string, std::string>> m_detected_enums;

    void perform_mutation(MiniZinc::BinOp* op, std::span<const MiniZinc::BinOpType> operators, std::string_view operator_name);
    void perform_mutation_unop(MiniZinc::BinOp* op, std::string_view operator_name);
    void perform_mutation_unop(MiniZinc::Call* call, std::string_view operator_name);
    void perform_mutation(MiniZinc::Call* call, std::span<const MiniZinc::ASTString> calls, std::string_view operator_name);
    void perform_call_swap_mutation(MiniZinc::Call* call);
};

} // namespace MuMiniZinc

#endif