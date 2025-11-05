cmake_minimum_required(VERSION 3.30)

SET(PROJECT_FANCY_NAME MuMiniZinc)
string(TOLOWER ${PROJECT_FANCY_NAME} PROJECT_NAME)
SET(PROJECT_NAME_LIB mumzn)

project(
  ${PROJECT_NAME}
  VERSION 0.0.1
  LANGUAGES CXX)

# Project properties
set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
set(CMAKE_CXX_CPPCHECK "cppcheck")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# No C++20 modules for now
cmake_policy(SET CMP0155 OLD)

configure_file(cmake/configured_files/config.hpp.in ${CMAKE_BINARY_DIR}/include/build/config.hpp ESCAPE_QUOTES)

add_library(${PROJECT_NAME_LIB}
    ${PROJECT_SOURCE_DIR}/src/arguments.cpp
    ${PROJECT_SOURCE_DIR}/src/mutation.cpp
    ${PROJECT_SOURCE_DIR}/src/executor.cpp
    ${PROJECT_SOURCE_DIR}/src/operators.cpp
)

target_include_directories(${PROJECT_NAME_LIB} PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME_LIB} PRIVATE "${CMAKE_BINARY_DIR}/include")

target_compile_definitions(${PROJECT_NAME_LIB} PRIVATE BOOST_PROCESS_USE_STD_FS=1)
target_compile_definitions(${PROJECT_NAME_LIB} PRIVATE BOOST_SYSTEM_USE_UTF8=1)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/src/main.cpp)

add_library(common_options INTERFACE)

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
            --coverage
            -ggdb3
            -fsanitize=address,leak,undefined>
        $<$<CONFIG:RELEASE>:
            -O3
            -DNDEBUG
            -march=native>>
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /utf-8
        /d1trimfile:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_options(common_options INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        $<$<CONFIG:DEBUG>:
            -fsanitize=address,leak,undefined --coverage>>)

target_link_libraries(${PROJECT_NAME_LIB} PRIVATE common_options)
target_link_libraries(${PROJECT_NAME} PRIVATE common_options)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME_LIB})

find_package(libminizinc REQUIRED)
find_package(Boost 1.88.0 REQUIRED COMPONENTS process)

target_link_libraries(${PROJECT_NAME} PRIVATE mzn)

foreach(dir IN LISTS libminizinc_INCLUDE_DIRS)
    list(APPEND _fixed_includes "${dir}/include")
endforeach()

set(libminizinc_INCLUDE_DIRS "${_fixed_includes}")
message("libminizinc: Using includes: ${libminizinc_INCLUDE_DIRS}")

include_directories(${libminizinc_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

list(APPEND PACKAGE_INCLUDE_DIRS "${libminizinc_INCLUDE_DIRS}/include")
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${PACKAGE_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::process)

# JSON
find_package(nlohmann_json 3.12.0 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
include_directories(${nlohmann_json_INCLUDE_DIRS})

# Testing framework
include(CTest)
add_subdirectory(tests)

# Doxygen
find_package(Doxygen OPTIONAL_COMPONENTS dot)

if (Doxygen_FOUND)
    set(DOXYGEN_HTML_COLORSTYLE "TOGGLE")
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

    doxygen_add_docs(
        docs 
        ${PROJECT_SOURCE_DIR}
    )
endif()