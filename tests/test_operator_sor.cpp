#define BOOST_TEST_MODULE test_operator_sor
#include <boost/test/included/unit_test.hpp>

#include <array>       // std::array
#include <string_view> // std::string_view

#include "test_operator_utils.hpp" // perform_test_execution, perform_test_operator, Status

namespace
{
constexpr std::array operator_to_test { ascii_ci_string_view { "SOR" } };
inline const auto path { data_path / "sor.mzn" };
}

BOOST_AUTO_TEST_CASE(sor)
{
    using namespace std::string_view_literals;

    constexpr std::array expected_mutants {
        R"(set of int: a = {1, 3, 6, 7, 8};
set of int: b = {3, 7, 8};
set of int: c = {1, 6};
constraint a subset b;
constraint b union c==a;
)"sv,
        R"(set of int: a = {1, 3, 6, 7, 8};
set of int: b = {3, 7, 8};
set of int: c = {1, 6};
constraint a superset b;
constraint b diff c==a;
)"sv,
        R"(set of int: a = {1, 3, 6, 7, 8};
set of int: b = {3, 7, 8};
set of int: c = {1, 6};
constraint a superset b;
constraint b symdiff c==a;
)"sv,
        R"(set of int: a = {1, 3, 6, 7, 8};
set of int: b = {3, 7, 8};
set of int: c = {1, 6};
constraint a superset b;
constraint b intersect c==a;
)"sv
    };

    perform_test_operator<operator_to_test.front()>(path, expected_mutants, 3);
}

BOOST_AUTO_TEST_CASE(sor_execution)
{
    constexpr std::array results {
        Status::Dead,
        Status::Dead,
        Status::Alive,
        Status::Dead
    };

    perform_test_execution(path, operator_to_test, {}, results, data_path / "sor-execution");
}