find_package(Boost 1.88 REQUIRED COMPONENTS unit_test_framework)

set(TEST_SOURCES
    test_mutant_arithmetic.cpp
    test_mutant_relational.cpp
    test_mutant_unary.cpp
    test_mutant_calls.cpp
)

foreach(test_file ${TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})

    target_link_libraries(${test_name}
        PRIVATE
            Boost::unit_test_framework
            ${PROJECT_NAME_LIB}
    )

    target_compile_definitions(${test_name} PRIVATE BOOST_TEST_DYN_LINK)

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()