#define BOOST_TEST_MODULE test_operator_cor
#include <boost/test/unit_test.hpp>

#include <array>       // std::array
#include <string_view> // std::string_view

#include "test_operator_utils.hpp" // perform_test_operator

namespace
{
inline constexpr std::string_view allowed_operator { "cor" };
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

    perform_test_operator<allowed_operator>("data/cor.mzn", expected_mutants);
}