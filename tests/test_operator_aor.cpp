#define BOOST_TEST_MODULE test_operator_aor
#include <boost/test/unit_test.hpp>

#include <array>       // std::array
#include <string_view> // std::string_view

#include "test_operator_utils.hpp" // perform_test_operator

namespace
{
inline constexpr std::string_view allowed_operator { "AOR" };
}

BOOST_AUTO_TEST_CASE(aor)
{
    using namespace std::string_view_literals;

    constexpr std::array expected_mutants {
        R"(int: value;
constraint 100-value < 150;
)"sv,
        R"(int: value;
constraint 100*value < 150;
)"sv,
        R"(int: value;
constraint 100/value < 150;
)"sv,
        R"(int: value;
constraint 100 div value < 150;
)"sv,
        R"(int: value;
constraint 100 mod value < 150;
)"sv,
        R"(int: value;
constraint 100^value < 150;
)"sv
    };

    perform_test_operator<allowed_operator>("data/aor.mzn", expected_mutants);
}