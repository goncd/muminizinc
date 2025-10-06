#ifndef ARGUMENTS_HPP
#define ARGUMENTS_HPP

#include <span>      // std::span
#include <stdexcept> // std::runtime_error

class BadArgument : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

int parse_arguments(std::span<const char*> argv);

#endif