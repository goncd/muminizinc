#define BOOST_TEST_MODULE test_operator_fcr
#include <boost/test/unit_test.hpp>

#include <array>       // std::array
#include <string_view> // std::string_view

#include "test_operator_utils.hpp" // perform_test_operator

namespace
{
inline constexpr std::string_view allowed_operator { "FCR" };
}

BOOST_AUTO_TEST_CASE(fcr)
{
    using namespace std::string_view_literals;

    constexpr std::array expected_mutants {
        R"(set of int: arr1 = 0..5;
set of int: arr2 = 0..5;
constraint exists ( x in arr1 ) ( x>=0 );
constraint exists ( x in arr2 ) ( x > 3 );
)"sv,
        R"(set of int: arr1 = 0..5;
set of int: arr2 = 0..5;
constraint forall ( x in arr1 ) ( x>=0 );
constraint forall ( x in arr2 ) ( x > 3 );
)"sv
    };

    perform_test_operator<allowed_operator>("data/fcr.mzn", expected_mutants, 1);
}