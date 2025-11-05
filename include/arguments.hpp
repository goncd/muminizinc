#ifndef ARGUMENTS_HPP
#define ARGUMENTS_HPP

#include <span>      // std::span
#include <stdexcept> // std::runtime_error

/**
 * @file
 * @brief Argument handling.
 */

/** Thrown when an unrecognized argument is detected. */
class BadArgument : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/** Parses the arguments given through `argv`. */
int parse_arguments(std::span<const char* const> argv);

#endif