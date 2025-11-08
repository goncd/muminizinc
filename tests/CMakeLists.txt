find_package(Boost ${MUMINIZINC_REQUIRED_BOOST_VERSION} REQUIRED COMPONENTS unit_test_framework)

set(TEST_SOURCES
    test_mutation.cpp
    test_operator_aor.cpp
    test_operator_cor.cpp
    test_operator_fas.cpp
    test_operator_fcr.cpp
    test_operator_ror.cpp
    test_operator_sor.cpp
    test_operator_uod.cpp
)

foreach(test_file ${TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})

    target_link_libraries(${test_name}
        PRIVATE
            Boost::unit_test_framework
            "MuMiniZinc::${PROJECT_NAME_LIB}"
            common_options
    )

    target_compile_definitions(${test_name} PRIVATE BOOST_TEST_DYN_LINK BOOST_PROCESS_USE_STD_FS=1)

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

add_executable(test_arguments test_arguments.cpp ../src/arguments.cpp)

target_link_libraries(test_arguments
    PRIVATE
        Boost::unit_test_framework
        "MuMiniZinc::${PROJECT_NAME_LIB}"
        common_options
)

target_compile_definitions(test_arguments PRIVATE BOOST_TEST_DYN_LINK BOOST_PROCESS_USE_STD_FS=1)

add_test(NAME test_arguments COMMAND test_arguments)

set(TEST_DATA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(TEST_DATA_DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/data")

# Copy the entire data directory to the build directory for the test executables,
# making sure old files are deleted first.
file(REMOVE_RECURSE ${TEST_DATA_DESTINATION})
file(COPY ${TEST_DATA_SOURCE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})