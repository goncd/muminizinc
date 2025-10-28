#define BOOST_TEST_MODULE test_mutant_sor
#include <boost/test/unit_test.hpp>

#include <array>       // std::array
#include <filesystem>  // std::filesystem::path
#include <string_view> // std::string_view

#include "test_operator_utils.hpp" // perform_test_operator

namespace
{
inline constexpr std::string_view allowed_operator { "SOR" };
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

    perform_test_operator<allowed_operator>("data/sor.mzn", expected_mutants, 3);
}