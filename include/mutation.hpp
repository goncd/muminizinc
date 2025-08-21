#ifndef MUTATION_HPP
#define MUTATION_HPP

#include <filesystem>
#include <string_view>

#include <minizinc/model.hh> // MiniZinc::Model

#include <boost/filesystem/path.hpp> // boost::filesystem::path

class MutationModel
{
public:
    explicit MutationModel(const std::filesystem::path& path, std::string_view output_directory);

    void find_mutants();

    void clear_output_folder() const;

    void run_mutants(const boost::filesystem::path& compiler_path, std::span<const char*> compiler_arguments) const;

private:
    class Mutator;
    void save_current_model(std::string_view mutant_name, std::uint64_t mutant_id, std::uint64_t occurrence_id) const;

    std::filesystem::path m_model_path, m_mutation_folder_path;
    std::string m_filename_stem;

    MiniZinc::Model* m_model = nullptr;

    static constexpr auto EXTENSION = std::string_view { ".mzn" };
    static constexpr auto WIDTH_PRINTER = 80;
};

#endif