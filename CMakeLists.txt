cmake_minimum_required(VERSION 3.30)

SET(PROJECT_NAME muminizinc)

project(
  ${PROJECT_NAME}
  VERSION 0.0.1
  DESCRIPTION "MuMiniZinc"
  LANGUAGES CXX)

file(GLOB SOURCE
    ${PROJECT_SOURCE_DIR}/src/arguments.cpp
    ${PROJECT_SOURCE_DIR}/src/main.cpp
    ${PROJECT_SOURCE_DIR}/src/mutation.cpp)

# Clang-tidy
set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# No C++20 modules for now
cmake_policy(SET CMP0155 OLD)

add_executable(${PROJECT_NAME} ${SOURCE})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_SOURCE_DIR}/include")

# Compiler configuration
target_compile_options(${PROJECT_NAME} PRIVATE
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
            -ggdb3
            -DDEBUG
            -fsanitize=address,leak,undefined>
        $<$<CONFIG:RELEASE>:
            -O3
            -DNDEBUG
            -march=native>>
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4>)

target_link_options(${PROJECT_NAME} PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        $<$<CONFIG:DEBUG>:
            -fsanitize=address,leak,undefined>>)

find_package(libminizinc REQUIRED)
find_package(Boost 1.64 REQUIRED COMPONENTS process)

target_link_libraries(${PROJECT_NAME} PRIVATE mzn)

foreach(dir IN LISTS libminizinc_INCLUDE_DIRS)
    list(APPEND _fixed_includes "${dir}/include")
endforeach()

set(libminizinc_INCLUDE_DIRS "${_fixed_includes}")
message("libminizinc: Using includes: ${libminizinc_INCLUDE_DIRS}")

include_directories(${libminizinc_INCLUDE_DIRS})

list(APPEND PACKAGE_INCLUDE_DIRS "${libminizinc_INCLUDE_DIRS}/include")
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${PACKAGE_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::process)
