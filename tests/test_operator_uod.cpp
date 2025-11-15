#define BOOST_TEST_MODULE test_operator_uod
#include <boost/test/included/unit_test.hpp>

#include <array>       // std::array
#include <string>      // std::string_literals
#include <string_view> // std::string_view

#include "test_operator_utils.hpp" // perform_test_execution, perform_test_operator, Status

namespace
{
inline constexpr std::array operator_to_test { ascii_ci_string_view { "UOD" } };
inline const auto path { data_path / "uod.mzn" };
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

    perform_test_operator<operator_to_test.front()>(path, expected_mutants, 1);
}

BOOST_AUTO_TEST_CASE(uod_execution)
{
    using namespace std::string_literals;

    const std::array data_files {
        "data/uod-1.dzn"s,
    };

    constexpr std::array results {
        Status::Alive,
        Status::Alive,
        Status::Dead,
    };

    perform_test_execution(path, operator_to_test, data_files, results, data_path / "uod-execution");
}