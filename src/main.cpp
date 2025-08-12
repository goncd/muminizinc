#include <exception> // std::exception
#include <format>    // std::format
#include <iostream>  // std::cerr, std::cout
#include <print>     // std::println
#include <span>      // std::span

#include <minizinc/exception.hh> // MiniZinc::Exception

#include <arguments.hpp> // parse_arguments

int main(int argc, const char** argv)
try
{
    return parse_arguments(std::span { argv, argv + argc });
}
catch (const MiniZinc::Exception& e)
{
    std::println(std::cerr, "MiniZinc compiler error: {:s}", e.msg());

    return 1;
}
catch (const std::exception& e)
{
    std::println(std::cerr, "Error: {:s}", e.what());

    return 1;
}
catch (...)
{
    std::println(std::cerr, "Unknown error");

    return 1;
}
