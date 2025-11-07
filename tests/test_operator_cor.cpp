#define BOOST_TEST_MODULE test_operator_cor
#include <boost/test/unit_test.hpp>

#include <array>       // std::array
#include <string_view> // std::string_view

#include "test_operator_utils.hpp" // perform_test_execution, perform_test_operator, Status

namespace
{
inline constexpr std::array operator_to_test { ascii_ci_string_view { "COR" } };
inline const auto path { data_path / "cor.mzn" };
}

BOOST_AUTO_TEST_CASE(cor)
{
    using namespace std::string_view_literals;

    constexpr std::array expected_mutants {
        R"(bool: a = true;
constraint a -> true;
)"sv,
        R"(bool: a = true;
constraint a <- true;
)"sv,
        R"(bool: a = true;
constraint a \/ true;
)"sv,
        R"(bool: a = true;
constraint a /\ true;
)"sv,
        R"(bool: a = true;
constraint a xor true;
)"sv
    };

    perform_test_operator<operator_to_test.front()>(path, expected_mutants);
}

BOOST_AUTO_TEST_CASE(cor_execution)
{
    constexpr std::array results {
        Status::Alive,
        Status::Alive,
        Status::Alive,
        Status::Alive,
        Status::Dead
    };

    perform_test_execution(path, operator_to_test, {}, results, data_path / "cor-execution");
}