#ifndef MUTATION_HPP
#define MUTATION_HPP

#include <filesystem>  // std::filesystem::path
#include <string_view> // std::string_view

#include <minizinc/model.hh> // MiniZinc::Model

#include <boost/filesystem/path.hpp> // boost::filesystem::path

class MutationModel
{
public:
    // Constructs a MutationModel that works only in memory.
    explicit MutationModel(const std::filesystem::path& path);

    // Constructs a MutationModel that works with a filesystem.
    explicit MutationModel(const std::filesystem::path& path, std::string_view output_directory);

    bool find_mutants();

    void clear_output_folder();

    void run_mutants(const boost::filesystem::path& compiler_path, std::span<std::string_view> compiler_arguments) const;

private:
    static constexpr auto EXTENSION = std::string_view { ".mzn" };
    static constexpr auto WIDTH_PRINTER = 80;

    class Mutator;
    void save_current_model(std::string_view mutant_name, std::uint64_t mutant_id, std::uint64_t occurrence_id);

    std::filesystem::path m_model_path, m_mutation_folder_path;
    std::string m_filename_stem;

    MiniZinc::Model* m_model = nullptr;

    // It's guaranteed that the first element is the original mutant.
    std::vector<std::pair<std::string, std::string>> m_memory;
};

#endif