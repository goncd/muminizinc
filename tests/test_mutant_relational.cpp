#define BOOST_TEST_MODULE test_mutant_relational

#include <boost/test/unit_test.hpp>

#include <algorithm>   // std::ranges::any_of
#include <array>       // std::array
#include <string_view> // std::string_view

#include <case_insensitive_string.hpp> // ascii_ci_string_view
#include <mutation.hpp>                // MutationModel

BOOST_AUTO_TEST_CASE(relational)
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

    MutationModel mutation_model { "data/relational.mzn", std::array { ascii_ci_string_view { "REL" } } };

    BOOST_REQUIRE_NO_THROW(BOOST_REQUIRE(mutation_model.find_mutants()));

    // Ignore the original mutant.
    const auto entries = mutation_model.get_memory().subspan(1);

    BOOST_REQUIRE(entries.size() == expected_mutants.size());

    for (auto mutant : expected_mutants)
        BOOST_REQUIRE(std::ranges::any_of(entries, [mutant](const auto& entry)
            { return entry.contents == mutant; }));
}