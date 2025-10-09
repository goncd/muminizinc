#define BOOST_TEST_MODULE test_mutant_call_swap

#include <boost/test/unit_test.hpp>

#include <algorithm>   // std::ranges::any_of
#include <array>       // std::array
#include <string_view> // std::string_view

#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <mutation.hpp>                // MutationModel

BOOST_AUTO_TEST_CASE(call_swap)
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

    MutationModel mutation_model { "data/call_swap.mzn", std::array { ascii_ci_string_view { "CALL" } } };

    BOOST_REQUIRE_NO_THROW(BOOST_REQUIRE(mutation_model.find_mutants()));

    // Ignore the original mutant.
    const auto entries = mutation_model.get_entries().subspan(1);

    BOOST_REQUIRE(entries.size() == expected_mutants.size());

    for (auto mutant : expected_mutants)
        BOOST_REQUIRE(std::ranges::any_of(entries, [mutant](const auto& entry)
            { return entry.contents == mutant; }));
}