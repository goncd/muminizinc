#include <cstdlib>   // EXIT_FAILURE, EXIT_SUCCESS
#include <exception> // std::exception
#include <iostream>  // std::cerr
#include <print>     // std::println
#include <span>      // std::span

#include <minizinc/exception.hh> // MiniZinc::Exception

#include <arguments.hpp> // parse_arguments

int main(int argc, const char** argv)
{
    try
    {
        return parse_arguments(std::span { argv, argv + argc });
    }
    catch (const MiniZinc::Exception& e)
    {
        std::println(std::cerr, "MiniZinc compiler error: {:s}", e.msg());

        return EXIT_FAILURE;
    }
    catch (const std::exception& e)
    {
        std::println(std::cerr, "Error: {:s}", e.what());

        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::println(std::cerr, "Unknown error");

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}