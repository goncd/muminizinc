#define BOOST_TEST_MODULE test_arguments
#include <boost/test/unit_test.hpp>

#include <array> // std::array
#include <span>  // std::span

#include <arguments.hpp> // BadArgument, parse_arguments

BOOST_AUTO_TEST_CASE(test_arguments)
{
    BOOST_CHECK_NO_THROW(parse_arguments({}));
    BOOST_REQUIRE_THROW(parse_arguments(std::array { "test", "unknown_argument" }), BadArgument);
}
