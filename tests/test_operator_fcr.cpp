#define BOOST_TEST_MODULE test_operator_fcr
#include <boost/test/included/unit_test.hpp>

#include <array>       // std::array
#include <string_view> // std::string_view

#include "test_operator_utils.hpp" // perform_test_execution, perform_test_operator, Status

namespace
{
inline constexpr std::array operator_to_test { ascii_ci_string_view { "FCR" } };
inline const auto path { data_path / "fcr.mzn" };
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

    perform_test_operator<operator_to_test.front()>(path, expected_mutants, 1);
}

BOOST_AUTO_TEST_CASE(fcr_execution)
{
    constexpr std::array results {
        Status::Alive,
        Status::Dead,
    };

    perform_test_execution(path, operator_to_test, {}, results, data_path / "fcr-execution");
}
