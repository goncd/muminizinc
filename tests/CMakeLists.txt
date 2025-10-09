find_package(Boost 1.88 REQUIRED COMPONENTS unit_test_framework)

set(TEST_SOURCES
    test_arguments.cpp
    test_execution.cpp
    test_mutant_arithmetic.cpp
    test_mutant_call_argument_swap.cpp
    test_mutant_call_swap.cpp
    test_mutation_model.cpp
    test_mutant_relational.cpp
    test_mutant_unary.cpp
)

foreach(test_file ${TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})

    target_link_libraries(${test_name}
        PRIVATE
            Boost::unit_test_framework
            ${PROJECT_NAME_LIB}
            Boost::process
            mzn
    )

    target_compile_definitions(${test_name} PRIVATE BOOST_TEST_DYN_LINK)
    target_compile_definitions(${test_name} PRIVATE BOOST_PROCESS_USE_STD_FS=1)

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

set(TEST_DATA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(TEST_DATA_DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/data")

# Copy the entire data directory to the build directory for the test executables,
# making sure old files are deleted first.
file(REMOVE_RECURSE ${TEST_DATA_DESTINATION})
file(COPY ${TEST_DATA_SOURCE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})