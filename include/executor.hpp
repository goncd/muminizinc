#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

#include <chrono>      // std::chrono::seconds
#include <span>        // std::span
#include <string_view> // std::string_view

#include <boost/filesystem/path.hpp> // boost::filesystem::path

#include <mutation.hpp> // MutationModel::Entry

void execute_mutants(const boost::filesystem::path& path, std::span<const std::string_view> compiler_arguments, std::span<const std::string_view> data_files, std::span<MutationModel::Entry> models, std::chrono::seconds timeout, std::uint64_t n_jobs);

#endif