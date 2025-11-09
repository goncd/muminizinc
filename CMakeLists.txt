cmake_minimum_required(VERSION 3.30)

SET(PROJECT_FANCY_NAME MuMiniZinc)
SET(PROJECT_NAMESPACE ${PROJECT_FANCY_NAME})
string(TOLOWER ${PROJECT_FANCY_NAME} PROJECT_NAME)
SET(PROJECT_NAME_LIB mumzn)

project(
  ${PROJECT_NAME}
  VERSION 1.0.0
  LANGUAGES CXX)

# Project options
option(MUMINIZINC_ENABLE_CLANG_TIDY "Enable clang-tidy static analysis." ON)
option(MUMINIZINC_ENABLE_CPPCHECK "Enable cppcheck static analysis." ON)
option(MUMINIZINC_BUILD_TESTS "Build tests." ON)
option(MUMINIZINC_BUILD_DOCS "Build internal documentation." ON)
option(MUMINIZINC_USE_GCOVR "Enable coverage testing." ON)
option(MUMINIZINC_USE_SANITIZERS "Use sanitizers on debug builds." ON)

if(MUMINIZINC_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE clang-tidy)
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy not found, disabling clang-tidy analysis")
        set(ENABLE_CLANG_TIDY OFF)
    endif()
endif()

if(MUMINIZINC_ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXE cppcheck)
    if(CPPCHECK_EXE)
        message(STATUS "cppcheck found: ${CPPCHECK_EXE}")
        set(CMAKE_CXX_CPPCHECK "${CPPCHECK_EXE};--enable=style;--suppress=unknownMacro")
    else()
        message(WARNING "cppcheck not found, disabling cppcheck analysis")
        set(ENABLE_CPPCHECK OFF)
    endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

configure_file(cmake/config.hpp.in ${CMAKE_BINARY_DIR}/include/muminizinc/build/config.hpp ESCAPE_QUOTES)

add_library(${PROJECT_NAME_LIB})
add_library("${PROJECT_NAMESPACE}::${PROJECT_NAME_LIB}" ALIAS ${PROJECT_NAME_LIB})

target_sources(
    ${PROJECT_NAME_LIB}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src/mutation.cpp
    ${PROJECT_SOURCE_DIR}/src/executor.cpp
    ${PROJECT_SOURCE_DIR}/src/operators.cpp
)

target_include_directories(
    ${PROJECT_NAME_LIB}
    PRIVATE src
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(${PROJECT_NAME_LIB} PRIVATE BOOST_PROCESS_USE_STD_FS=1 BOOST_SYSTEM_USE_UTF8=1)

add_library(common_options INTERFACE)

target_compile_features(common_options INTERFACE cxx_std_23)

# Compiler configuration
target_compile_options(common_options INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -Wall
        -Wextra
        -Wconversion
        -pedantic
        -Wfatal-errors
        -fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=
        -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=
        $<$<CONFIG:DEBUG>:
            -Og
            -ggdb3>
        $<$<CONFIG:RELEASE>:
            -march=native>>
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /utf-8
        /d1trimfile:${CMAKE_CURRENT_SOURCE_DIR}>)

if(MUMINIZINC_USE_SANITIZERS)
    target_compile_options(common_options INTERFACE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
            $<$<CONFIG:DEBUG>:
                -fsanitize=address,leak,undefined>>
        $<$<CXX_COMPILER_ID:AppleClang>:
            $<$<CONFIG:DEBUG>:
                -fsanitize=address,undefined>>
        $<$<CXX_COMPILER_ID:MSVC>:
            $<$<CONFIG:DEBUG>:
                /fsanitize=address>>)

    target_link_options(common_options INTERFACE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
            $<$<CONFIG:DEBUG>:
                -fsanitize=address,leak,undefined>>
        $<$<CXX_COMPILER_ID:AppleClang>:
            $<$<CONFIG:DEBUG>:
                -fsanitize=address,undefined>>)
endif()

if(MUMINIZINC_USE_GCOVR
    AND ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")))
    find_program(GCOVR gcovr)

    if(GCOVR)
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
            COMMAND ${CMAKE_CTEST_COMMAND}
            COMMAND ${GCOVR} -r ${CMAKE_SOURCE_DIR} --html-details -o coverage/coverage.html
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running tests and generating HTML coverage report"
        )

        target_compile_options(common_options INTERFACE --coverage)
        target_link_options(common_options INTERFACE --coverage)
    endif()
endif()

target_link_libraries(${PROJECT_NAME_LIB} PUBLIC common_options)

find_package(libminizinc REQUIRED)

foreach(dir IN LISTS libminizinc_INCLUDE_DIRS)
    list(APPEND _fixed_includes "${dir}/include")
endforeach()

set(libminizinc_INCLUDE_DIRS "${_fixed_includes}")
message(STATUS "libminizinc: Using includes: ${libminizinc_INCLUDE_DIRS}")

target_include_directories(${PROJECT_NAME_LIB} SYSTEM PUBLIC ${libminizinc_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME_LIB} PUBLIC mzn)

set(MUMINIZINC_REQUIRED_BOOST_VERSION 1.88.0)
find_package(Boost ${MUMINIZINC_REQUIRED_BOOST_VERSION} REQUIRED COMPONENTS process)
target_link_libraries(${PROJECT_NAME_LIB} PUBLIC Boost::process)

set(MUMINIZINC_REQUIRED_NLOHMANN_JSON_VERSION 3.12.0)
find_package(nlohmann_json ${MUMINIZINC_REQUIRED_NLOHMANN_JSON_VERSION} REQUIRED)
target_link_libraries(${PROJECT_NAME_LIB} PUBLIC nlohmann_json::nlohmann_json)

install(
    TARGETS ${PROJECT_NAME_LIB}
    EXPORT "${PROJECT_FANCY_NAME}Targets"
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    TARGETS common_options
    EXPORT "${PROJECT_FANCY_NAME}Targets"
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${PROJECT_FANCY_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/${PROJECT_FANCY_NAME}Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_FANCY_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/${PROJECT_NAME}"
)

install(
    EXPORT "${PROJECT_FANCY_NAME}Targets"
    FILE "${PROJECT_FANCY_NAME}Targets.cmake"
    NAMESPACE "${PROJECT_NAMESPACE}::"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/${PROJECT_NAME}
)

export(
    EXPORT "${PROJECT_FANCY_NAME}Targets"
    FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_FANCY_NAME}Targets.cmake"
    NAMESPACE "${PROJECT_NAMESPACE}::"
)

if(MUMINIZINC_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

if(MUMINIZINC_BUILD_DOCS)
    find_package(Doxygen OPTIONAL_COMPONENTS dot)

    if (Doxygen_FOUND)
        set(DOXYGEN_HTML_COLORSTYLE "TOGGLE")
        set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

        doxygen_add_docs(
            docs 
            ${PROJECT_SOURCE_DIR}
        )
    endif()
endif()

add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCE_DIR}/src/arguments.cpp
    ${PROJECT_SOURCE_DIR}/src/main.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE BOOST_PROCESS_USE_STD_FS=1 BOOST_SYSTEM_USE_UTF8=1)
target_link_libraries(${PROJECT_NAME} PRIVATE common_options Boost::process ${PROJECT_NAME_LIB})
