#define BOOST_TEST_MODULE test_operator_ror
#include <boost/test/unit_test.hpp>

#include <array>       // std::array
#include <string_view> // std::string_view

#include "test_operator_utils.hpp" // perform_test_operator

namespace
{
inline constexpr std::string_view allowed_operator { "ROR" };
}

BOOST_AUTO_TEST_CASE(ror)
{
    using namespace std::string_view_literals;

    constexpr std::array expected_mutants {
        R"(int: value;
constraint value<=5;
)"sv,
        R"(int: value;
constraint value > 5;
)"sv,
        R"(int: value;
constraint value>=5;
)"sv,
        R"(int: value;
constraint value==5;
)"sv,
        R"(int: value;
constraint value!=5;
)"sv,
    };

    perform_test_operator<allowed_operator>("data/ror.mzn", expected_mutants);
}
