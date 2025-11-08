#include <cstdlib>   // EXIT_FAILURE, EXIT_SUCCESS
#include <exception> // std::exception
#include <iostream>  // std::cerr
#include <print>     // std::println
#include <span>      // std::span

#include <minizinc/exception.hh> // MiniZinc::Exception

#include <arguments.hpp>          // parse_arguments
#include <muminizinc/logging.hpp> // logging::color_support::check, logging::code, logging::Color, logging::OutputType,logging::Style,

int main(int argc, const char** argv)
{
    try
    {
        logging::color_support::check();

        return parse_arguments(std::span { argv, argv + argc });
    }
    catch (const MiniZinc::Exception& e)
    {
        std::println(std::cerr, "{}{}MiniZinc compiler error{}:", logging::code<logging::OutputType::StandardError>(logging::Style::Bold), logging::code<logging::OutputType::StandardError>(logging::Color::Red), logging::code<logging::OutputType::StandardError>(logging::Style::Reset));

        e.print(std::cerr);

        return EXIT_FAILURE;
    }
    catch (const std::exception& e)
    {
        std::println(std::cerr, "{}{}Error{}: {:s}", logging::code<logging::OutputType::StandardError>(logging::Style::Bold), logging::code<logging::OutputType::StandardError>(logging::Color::Red), logging::code<logging::OutputType::StandardError>(logging::Style::Reset), e.what());

        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::println(std::cerr, "{}{}Unknown error{}", logging::code<logging::OutputType::StandardError>(logging::Style::Bold), logging::code<logging::OutputType::StandardError>(logging::Color::Red), logging::code<logging::OutputType::StandardError>(logging::Style::Reset));

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}