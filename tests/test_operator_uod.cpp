#define BOOST_TEST_MODULE test_operator_uod
#include <boost/test/unit_test.hpp>

#include <array>       // std::array
#include <string_view> // std::string_view

#include "test_operator_utils.hpp" // perform_test_operator

namespace
{
inline constexpr std::string_view allowed_operator { "UOD" };
}

BOOST_AUTO_TEST_CASE(uod)
{
    using namespace std::string_view_literals;

    constexpr std::array expected_mutants {
        R"(int: value;
constraint value < 150;
constraint 150 > -value;
output  ["negative value is "++format(-value)++"\n"];
)"sv,
        R"(int: value;
constraint -value < 150;
constraint 150 > value;
output  ["negative value is "++format(-value)++"\n"];
)"sv,
        R"(int: value;
constraint -value < 150;
constraint 150 > -value;
output  ["negative value is "++format(value)++"\n"];
)"sv
    };

    perform_test_operator<allowed_operator>("data/uod.mzn", expected_mutants, 1);
}